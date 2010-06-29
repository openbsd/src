/*	$OpenBSD: ntfs_vfsops.c,v 1.16 2010/06/29 04:09:32 tedu Exp $	*/
/*	$NetBSD: ntfs_vfsops.c,v 1.7 2003/04/24 07:50:19 christos Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Id: ntfs_vfsops.c,v 1.7 1999/05/31 11:28:30 phk Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <uvm/uvm_extern.h>
#else
#include <vm/vm.h>
#endif

#include <miscfs/specfs/specdev.h>

/*#define NTFS_DEBUG 1*/
#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <fs/ntfs/ntfs.h>
#include <fs/ntfs/ntfs_inode.h>
#include <fs/ntfs/ntfs_subr.h>
#include <fs/ntfs/ntfs_vfsops.h>
#include <fs/ntfs/ntfs_ihash.h>
#include <fs/ntfs/ntfsmount.h>
#else
#include <ntfs/ntfs.h>
#include <ntfs/ntfs_inode.h>
#include <ntfs/ntfs_subr.h>
#include <ntfs/ntfs_vfsops.h>
#include <ntfs/ntfs_ihash.h>
#include <ntfs/ntfsmount.h>
#endif

#ifdef MALLOC_DEFINE
MALLOC_DEFINE(M_NTFSMNT, "NTFS mount", "NTFS mount structure");
MALLOC_DEFINE(M_NTFSNTNODE,"NTFS ntnode",  "NTFS ntnode information");
MALLOC_DEFINE(M_NTFSFNODE,"NTFS fnode",  "NTFS fnode information");
MALLOC_DEFINE(M_NTFSDIR,"NTFS dir",  "NTFS dir buffer");
#endif

#if defined(__FreeBSD__)
static int	ntfs_mount(struct mount *, char *, caddr_t,
				struct nameidata *, struct proc *);
#else
static int	ntfs_mount(struct mount *, const char *, void *,
				struct nameidata *, struct proc *);
#endif
static int	ntfs_quotactl(struct mount *, int, uid_t, caddr_t,
				   struct proc *);
static int	ntfs_root(struct mount *, struct vnode **);
static int	ntfs_start(struct mount *, int, struct proc *);
static int	ntfs_statfs(struct mount *, struct statfs *,
				 struct proc *);
static int	ntfs_sync(struct mount *, int, struct ucred *,
			       struct proc *);
static int	ntfs_unmount(struct mount *, int, struct proc *);
static int	ntfs_vget(struct mount *mp, ino_t ino,
			       struct vnode **vpp);
static int	ntfs_mountfs(struct vnode *, struct mount *, 
				  struct ntfs_args *, struct proc *);
static int	ntfs_vptofh(struct vnode *, struct fid *);

static int	ntfs_init(struct vfsconf *);
static int	ntfs_fhtovp(struct mount *, struct fid *,
   			     struct vnode **);
static int	ntfs_checkexp(struct mount *, struct mbuf *,
			       int *, struct ucred **);
static int	ntfs_sysctl(int *, u_int, void *, size_t *, void *,
 			     size_t, struct proc *);

/*
 * Verify a remote client has export rights and return these rights via.
 * exflagsp and credanonp.
 */
static int
ntfs_checkexp(mp, nam, exflagsp, credanonp)
	struct mount *mp;
	struct mbuf *nam;
	int *exflagsp;
	struct ucred **credanonp;
{
	struct netcred *np;
	struct ntfsmount *ntm = VFSTONTFS(mp);

	/*
	 * Get the export permission structure for this <mp, client> tuple.
	 */
	np = vfs_export_lookup(mp, &ntm->ntm_export, nam);
	if (np == NULL)
		return (EACCES);

	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return (0);
}

/*ARGSUSED*/
static int
ntfs_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	return (EINVAL);
}

static int
ntfs_init (
	struct vfsconf *vcp )
{
	ntfs_nthashinit();
	ntfs_toupper_init();
	return 0;
}

static int
ntfs_mount( 
	struct mount *mp,
	const char *path,
	void *data,
	struct nameidata *ndp,
	struct proc *p )
{
	int		err = 0;
	struct vnode	*devvp;
	struct ntfs_args args;
	size_t size;
	mode_t amode;

	/*
	 ***
	 * Mounting non-root file system or updating a file system
	 ***
	 */

	/* copy in user arguments*/
	err = copyin(data, (caddr_t)&args, sizeof (struct ntfs_args));
	if (err)
		goto error_1;		/* can't get arguments*/

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		/* if not updating name...*/
		if (args.fspec == 0) {
			/*
			 * Process export requests.  Jumping to "success"
			 * will return the vfs_export() error code.
			 */
			struct ntfsmount *ntm = VFSTONTFS(mp);
			err = vfs_export(mp, &ntm->ntm_export, &args.export_info);
			goto success;
		}

		printf("ntfs_mount(): MNT_UPDATE not supported\n");
		err = EINVAL;
		goto error_1;
	}

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	err = namei(ndp);
	if (err) {
		/* can't get devvp!*/
		goto error_1;
	}

	devvp = ndp->ni_vp;

	if (devvp->v_type != VBLK) {
		err = ENOTBLK;
		goto error_2;
	}

	if (major(devvp->v_rdev) >= nblkdev) {
		err = ENXIO;
		goto error_2;
	}

	/*
	 * If we are not root, make sure we have permission to access the
	 * requested device.
	 */
	if (p->p_ucred->cr_uid) {
		amode = (mp->mnt_flag & MNT_RDONLY) ? VREAD : (VREAD | VWRITE);
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
		err = VOP_ACCESS(devvp, amode, p->p_ucred, p);
		VOP_UNLOCK(devvp, 0, p);
		if (err)
			goto error_2;
	}

	if (mp->mnt_flag & MNT_UPDATE) {
#if 0
		/*
		 ********************
		 * UPDATE
		 ********************
		 */

		if (devvp != ntmp->um_devvp)
			err = EINVAL;	/* needs translation */
		else
			vrele(devvp);
		/*
		 * Update device name only on success
		 */
		if( !err) {
			err = set_statfs_info(NULL, UIO_USERSPACE, args.fspec,
			    UIO_USERSPACE, mp, p);
		}
#endif
	} else {
		/*
		 ********************
		 * NEW MOUNT
		 ********************
		 */

		/*
		 * Since this is a new mount, we want the names for
		 * the device and the mount point copied in.  If an
		 * error occurs,  the mountpoint is discarded by the
		 * upper level code.
		 */
		/* Save "last mounted on" info for mount point (NULL pad)*/
		(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1,
		           &size);
		bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
		(void) copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, 
		           MNAMELEN - 1, &size);
		bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
		bcopy(&args, &mp->mnt_stat.mount_info.ntfs_args, sizeof(args));
		if ( !err) {
			err = ntfs_mountfs(devvp, mp, &args, p);
		}
	}
	if (err) {
		goto error_2;
	}

	/*
	 * Initialize FS stat information in mount struct; uses both
	 * mp->mnt_stat.f_mntonname and mp->mnt_stat.f_mntfromname
	 *
	 * This code is common to root and non-root mounts
	 */
	(void)VFS_STATFS(mp, &mp->mnt_stat, p);

	goto success;


error_2:	/* error with devvp held*/

	/* release devvp before failing*/
	vrele(devvp);

error_1:	/* no state to back out*/

success:
	return(err);
}

/*
 * Common code for mount and mountroot
 */
int
ntfs_mountfs(devvp, mp, argsp, p)
	struct vnode *devvp;
	struct mount *mp;
	struct ntfs_args *argsp;
	struct proc *p;
{
	struct buf *bp;
	struct ntfsmount *ntmp = NULL;
	dev_t dev = devvp->v_rdev;
	int error, ronly, ncount, i;
	struct vnode *vp;

	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	error = vfs_mountedon(devvp);
	if (error)
		return (error);
	ncount = vcount(devvp);
	if (ncount > 1 && devvp != rootvp)
		return (EBUSY);
	error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0);
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p);
	if (error)
		return (error);

	bp = NULL;

	error = bread(devvp, BBLOCK, BBSIZE, NOCRED, &bp);
	if (error)
		goto out;
	ntmp = malloc(sizeof *ntmp, M_NTFSMNT, M_WAITOK | M_ZERO);
	bcopy(bp->b_data, &ntmp->ntm_bootfile, sizeof(struct bootfile));
	brelse(bp);
	bp = NULL;

	if (strncmp(ntmp->ntm_bootfile.bf_sysid, NTFS_BBID, NTFS_BBIDLEN)) {
		error = EINVAL;
		dprintf(("ntfs_mountfs: invalid boot block\n"));
		goto out;
	}

	{
		int8_t cpr = ntmp->ntm_mftrecsz;
		if( cpr > 0 )
			ntmp->ntm_bpmftrec = ntmp->ntm_spc * cpr;
		else
			ntmp->ntm_bpmftrec = (1 << (-cpr)) / ntmp->ntm_bps;
	}
	dprintf(("ntfs_mountfs(): bps: %d, spc: %d, media: %x, mftrecsz: %d (%d sects)\n",
		ntmp->ntm_bps,ntmp->ntm_spc,ntmp->ntm_bootfile.bf_media,
		ntmp->ntm_mftrecsz,ntmp->ntm_bpmftrec));
	dprintf(("ntfs_mountfs(): mftcn: 0x%x|0x%x\n",
		(u_int32_t)ntmp->ntm_mftcn,(u_int32_t)ntmp->ntm_mftmirrcn));

	ntmp->ntm_mountp = mp;
	ntmp->ntm_dev = dev;
	ntmp->ntm_devvp = devvp;
	ntmp->ntm_uid = argsp->uid;
	ntmp->ntm_gid = argsp->gid;
	ntmp->ntm_mode = argsp->mode;
	ntmp->ntm_flag = argsp->flag;
	mp->mnt_data = (qaddr_t) ntmp;

	/* set file name encode/decode hooks XXX utf-8 only for now */
	ntmp->ntm_wget = ntfs_utf8_wget;
	ntmp->ntm_wput = ntfs_utf8_wput;
	ntmp->ntm_wcmp = ntfs_utf8_wcmp;

	dprintf(("ntfs_mountfs(): case-%s,%s uid: %d, gid: %d, mode: %o\n",
		(ntmp->ntm_flag & NTFS_MFLAG_CASEINS)?"insens.":"sens.",
		(ntmp->ntm_flag & NTFS_MFLAG_ALLNAMES)?" allnames,":"",
		ntmp->ntm_uid, ntmp->ntm_gid, ntmp->ntm_mode));

	/*
	 * We read in some system nodes to do not allow 
	 * reclaim them and to have everytime access to them.
	 */ 
	{
		int pi[3] = { NTFS_MFTINO, NTFS_ROOTINO, NTFS_BITMAPINO };
		for (i=0; i<3; i++) {
			error = VFS_VGET(mp, pi[i], &(ntmp->ntm_sysvn[pi[i]]));
			if(error)
				goto out1;
			ntmp->ntm_sysvn[pi[i]]->v_flag |= VSYSTEM;
			vref(ntmp->ntm_sysvn[pi[i]]);
			vput(ntmp->ntm_sysvn[pi[i]]);
		}
	}

	/* read the Unicode lowercase --> uppercase translation table,
	 * if necessary */
	if ((error = ntfs_toupper_use(mp, ntmp, p)))
		goto out1;

	/*
	 * Scan $BitMap and count free clusters
	 */
	error = ntfs_calccfree(ntmp, &ntmp->ntm_cfree);
	if(error)
		goto out1;

	/*
	 * Read and translate to internal format attribute
	 * definition file. 
	 */
	{
		int num,j;
		struct attrdef ad;

		/* Open $AttrDef */
		error = VFS_VGET(mp, NTFS_ATTRDEFINO, &vp );
		if(error) 
			goto out1;

		/* Count valid entries */
		for(num=0;;num++) {
			error = ntfs_readattr(ntmp, VTONT(vp),
					NTFS_A_DATA, NULL,
					num * sizeof(ad), sizeof(ad),
					&ad, NULL);
			if (error)
				goto out1;
			if (ad.ad_name[0] == 0)
				break;
		}

		/* Alloc memory for attribute definitions */
		ntmp->ntm_ad = (struct ntvattrdef *) malloc(
			num * sizeof(struct ntvattrdef),
			M_NTFSMNT, M_WAITOK);

		ntmp->ntm_adnum = num;

		/* Read them and translate */
		for(i=0;i<num;i++){
			error = ntfs_readattr(ntmp, VTONT(vp),
					NTFS_A_DATA, NULL,
					i * sizeof(ad), sizeof(ad),
					&ad, NULL);
			if (error)
				goto out1;
			j = 0;
			do {
				ntmp->ntm_ad[i].ad_name[j] = ad.ad_name[j];
			} while(ad.ad_name[j++]);
			ntmp->ntm_ad[i].ad_namelen = j - 1;
			ntmp->ntm_ad[i].ad_type = ad.ad_type;
		}

		vput(vp);
	}

	mp->mnt_stat.f_fsid.val[0] = dev;
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_maxsymlinklen = 0;
	mp->mnt_flag |= MNT_LOCAL;
	devvp->v_specmountpoint = mp;
	return (0);

out1:
	for (i = 0; i < NTFS_SYSNODESNUM; i++)
		if (ntmp->ntm_sysvn[i])
			vrele(ntmp->ntm_sysvn[i]);

	if (vflush(mp,NULLVP,0))
		dprintf(("ntfs_mountfs: vflush failed\n"));

out:
	devvp->v_specmountpoint = NULL;
	if (bp)
		brelse(bp);

	if (ntmp != NULL) {
		if (ntmp->ntm_ad != NULL)
			free(ntmp->ntm_ad, M_NTFSMNT);
		free(ntmp, M_NTFSMNT);
		mp->mnt_data = NULL;
	}

	/* lock the device vnode before calling VOP_CLOSE() */
	VN_LOCK(devvp, LK_EXCLUSIVE | LK_RETRY, p);
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, p);
	VOP__UNLOCK(devvp, 0, p);
	
	return (error);
}

static int
ntfs_start (
	struct mount *mp,
	int flags,
	struct proc *p )
{
	return (0);
}

static int
ntfs_unmount( 
	struct mount *mp,
	int mntflags,
	struct proc *p)
{
	struct ntfsmount *ntmp;
	int error, ronly = 0, flags, i;

	dprintf(("ntfs_unmount: unmounting...\n"));
	ntmp = VFSTONTFS(mp);

	flags = 0;
	if(mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	dprintf(("ntfs_unmount: vflushing...\n"));
	error = vflush(mp,NULLVP,flags | SKIPSYSTEM);
	if (error) {
		dprintf(("ntfs_unmount: vflush failed: %d\n",error));
		return (error);
	}

	/* Check if only system vnodes are rest */
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		 if((ntmp->ntm_sysvn[i]) && 
		    (ntmp->ntm_sysvn[i]->v_usecount > 1)) return (EBUSY);

	/* Dereference all system vnodes */
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		 if(ntmp->ntm_sysvn[i]) vrele(ntmp->ntm_sysvn[i]);

	/* vflush system vnodes */
	error = vflush(mp,NULLVP,flags);
	if (error) {
		/* XXX should this be panic() ? */
		printf("ntfs_unmount: vflush failed(sysnodes): %d\n",error);
	}

	/* Check if the type of device node isn't VBAD before
	 * touching v_specinfo.  If the device vnode is revoked, the
	 * field is NULL and touching it causes null pointer derefercence.
	 */
	if (ntmp->ntm_devvp->v_type != VBAD)
		ntmp->ntm_devvp->v_specmountpoint = NULL;

	vinvalbuf(ntmp->ntm_devvp, V_SAVE, NOCRED, p, 0, 0);

	/* lock the device vnode before calling VOP_CLOSE() */
	VOP_LOCK(ntmp->ntm_devvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_CLOSE(ntmp->ntm_devvp, ronly ? FREAD : FREAD|FWRITE,
		NOCRED, p);
	VOP__UNLOCK(ntmp->ntm_devvp, 0, p);

	vrele(ntmp->ntm_devvp);

	/* free the toupper table, if this has been last mounted ntfs volume */
	ntfs_toupper_unuse(p);

	dprintf(("ntfs_umount: freeing memory...\n"));
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	free(ntmp->ntm_ad, M_NTFSMNT);
	free(ntmp, M_NTFSMNT);
	return (error);
}

static int
ntfs_root(
	struct mount *mp,
	struct vnode **vpp )
{
	struct vnode *nvp;
	int error = 0;

	dprintf(("ntfs_root(): sysvn: %p\n",
		VFSTONTFS(mp)->ntm_sysvn[NTFS_ROOTINO]));
	error = VFS_VGET(mp, (ino_t)NTFS_ROOTINO, &nvp);
	if(error) {
		printf("ntfs_root: VFS_VGET failed: %d\n",error);
		return (error);
	}

	*vpp = nvp;
	return (0);
}

/*
 * Do operations associated with quotas, not supported
 */
/* ARGSUSED */
static int
ntfs_quotactl ( 
	struct mount *mp,
	int cmds,
	uid_t uid,
	caddr_t arg,
	struct proc *p)
{

	return EOPNOTSUPP;
}

int
ntfs_calccfree(
	struct ntfsmount *ntmp,
	cn_t *cfreep)
{
	struct vnode *vp;
	u_int8_t *tmp;
	int j, error;
	cn_t cfree = 0;
	size_t bmsize, i;

	vp = ntmp->ntm_sysvn[NTFS_BITMAPINO];

	bmsize = VTOF(vp)->f_size;

	tmp = (u_int8_t *) malloc(bmsize, M_TEMP, M_WAITOK);

	error = ntfs_readattr(ntmp, VTONT(vp), NTFS_A_DATA, NULL,
			       0, bmsize, tmp, NULL);
	if (error)
		goto out;

	for(i=0;i<bmsize;i++)
		for(j=0;j<8;j++)
			if(~tmp[i] & (1 << j)) cfree++;
	*cfreep = cfree;

    out:
	free(tmp, M_TEMP);
	return(error);
}

static int
ntfs_statfs(
	struct mount *mp,
	struct statfs *sbp,
	struct proc *p)
{
	struct ntfsmount *ntmp = VFSTONTFS(mp);
	u_int64_t mftallocated;

	dprintf(("ntfs_statfs():\n"));

	mftallocated = VTOF(ntmp->ntm_sysvn[NTFS_MFTINO])->f_allocated;

	sbp->f_bsize = ntmp->ntm_bps;
	sbp->f_iosize = ntmp->ntm_bps * ntmp->ntm_spc;
	sbp->f_blocks = ntmp->ntm_bootfile.bf_spv;
	sbp->f_bfree = sbp->f_bavail = ntfs_cntobn(ntmp->ntm_cfree);
	sbp->f_ffree = sbp->f_bfree / ntmp->ntm_bpmftrec;
	sbp->f_files = mftallocated / ntfs_bntob(ntmp->ntm_bpmftrec) +
		       sbp->f_ffree;
	sbp->f_flags = mp->mnt_flag;
	if (sbp != &mp->mnt_stat) {
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
		bcopy(&mp->mnt_stat.mount_info.msdosfs_args,
		    &sbp->mount_info.msdosfs_args, sizeof(struct msdosfs_args));
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);

	return (0);
}

static int
ntfs_sync (
	struct mount *mp,
	int waitfor,
	struct ucred *cred,
	struct proc *p)
{
	/*dprintf(("ntfs_sync():\n"));*/
	return (0);
}

/*ARGSUSED*/
static int
ntfs_fhtovp(
	struct mount *mp,
	struct fid *fhp,
	struct vnode **vpp)
{
	struct ntfid *ntfhp = (struct ntfid *)fhp;
	int error;

	ddprintf(("ntfs_fhtovp(): %s: %d\n", mp->mnt_stat.f_mntonname,
		ntfhp->ntfid_ino));

	error = ntfs_vgetex(mp, ntfhp->ntfid_ino, ntfhp->ntfid_attr, NULL,
			LK_EXCLUSIVE | LK_RETRY, 0, curproc, vpp); /* XXX */
	if (error != 0) {
		*vpp = NULLVP;
		return (error);
	}

	/* XXX as unlink/rmdir/mkdir/creat are not currently possible
	 * with NTFS, we don't need to check anything else for now */
	return (0);
}

static int
ntfs_vptofh(
	struct vnode *vp,
	struct fid *fhp)
{
	struct ntnode *ntp;
	struct ntfid *ntfhp;
	struct fnode *fn;

	ddprintf(("ntfs_fhtovp(): %s: %p\n", vp->v_mount->mnt_stat.f_mntonname,
		vp));

	fn = VTOF(vp);
	ntp = VTONT(vp);
	ntfhp = (struct ntfid *)fhp;
	ntfhp->ntfid_len = sizeof(struct ntfid);
	ntfhp->ntfid_ino = ntp->i_number;
	ntfhp->ntfid_attr = fn->f_attrtype;
#ifdef notyet
	ntfhp->ntfid_gen = ntp->i_gen;
#endif
	return (0);
}

int
ntfs_vgetex(
	struct mount *mp,
	ino_t ino,
	u_int32_t attrtype,
	char *attrname,
	u_long lkflags,
	u_long flags,
	struct proc *p,
	struct vnode **vpp) 
{
	int error;
	struct ntfsmount *ntmp;
	struct ntnode *ip;
	struct fnode *fp;
	struct vnode *vp;
	enum vtype f_type;

	dprintf(("ntfs_vgetex: ino: %d, attr: 0x%x:%s, lkf: 0x%lx, f: 0x%lx\n",
		ino, attrtype, attrname?attrname:"", (u_long)lkflags,
		(u_long)flags ));

	ntmp = VFSTONTFS(mp);
	*vpp = NULL;

	/* Get ntnode */
	error = ntfs_ntlookup(ntmp, ino, &ip, p);
	if (error) {
		printf("ntfs_vget: ntfs_ntget failed\n");
		return (error);
	}

	/* It may be not initialized fully, so force load it */
	if (!(flags & VG_DONTLOADIN) && !(ip->i_flag & IN_LOADED)) {
		error = ntfs_loadntnode(ntmp, ip);
		if(error) {
			printf("ntfs_vget: CAN'T LOAD ATTRIBUTES FOR INO: %d\n",
			       ip->i_number);
			ntfs_ntput(ip, p);

			return (error);
		}
	}

	error = ntfs_fget(ntmp, ip, attrtype, attrname, &fp);
	if (error) {
		printf("ntfs_vget: ntfs_fget failed\n");
		ntfs_ntput(ip, p);

		return (error);
	}

	if (!(flags & VG_DONTVALIDFN) && !(fp->f_flag & FN_VALID)) {
		if ((ip->i_frflag & NTFS_FRFLAG_DIR) &&
		    (fp->f_attrtype == NTFS_A_DATA && fp->f_attrname == NULL)) {
			f_type = VDIR;
		} else if (flags & VG_EXT) {
			f_type = VNON;
			fp->f_size = fp->f_allocated = 0;
		} else {
			f_type = VREG;	

			error = ntfs_filesize(ntmp, fp, 
					      &fp->f_size, &fp->f_allocated);
			if (error) {
				ntfs_ntput(ip, p);

				return (error);
			}
		}

		fp->f_flag |= FN_VALID;
	}

	/*
	 * We may be calling vget() now. To avoid potential deadlock, we need
	 * to release ntnode lock, since due to locking order vnode
	 * lock has to be acquired first.
	 * ntfs_fget() bumped ntnode usecount, so ntnode won't be recycled
	 * prematurely.
	 */
	ntfs_ntput(ip, p);

	if (FTOV(fp)) {
		/* vget() returns error if the vnode has been recycled */
		if (VGET(FTOV(fp), lkflags, p) == 0) {
			*vpp = FTOV(fp);
			return (0);
		}
	}

	error = getnewvnode(VT_NTFS, ntmp->ntm_mountp, ntfs_vnodeop_p, &vp);
	if(error) {
		ntfs_frele(fp);
		ntfs_ntput(ip, p);

		return (error);
	}
	dprintf(("ntfs_vget: vnode: %p for ntnode: %d\n", vp,ino));

	fp->f_vp = vp;
	vp->v_data = fp;
	vp->v_type = f_type;

	if (ino == NTFS_ROOTINO)
		vp->v_flag |= VROOT;

	if (lkflags & LK_TYPE_MASK) {
		error = VN_LOCK(vp, lkflags, p);
		if (error) {
			vput(vp);
			return (error);
		}
	}

	*vpp = vp;
	return (0);
}

static int
ntfs_vget(
	struct mount *mp,
	ino_t ino,
	struct vnode **vpp) 
{
	return ntfs_vgetex(mp, ino, NTFS_A_DATA, NULL,
			LK_EXCLUSIVE | LK_RETRY, 0, curproc, vpp); /* XXX */
}

extern const struct vnodeopv_desc ntfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const ntfs_vnodeopv_descs[] = {
	&ntfs_vnodeop_opv_desc,
	NULL,
};

const struct vfsops ntfs_vfsops = {
	ntfs_mount,
	ntfs_start,
	ntfs_unmount,
	ntfs_root,
	ntfs_quotactl,
	ntfs_statfs,
	ntfs_sync,
	ntfs_vget,
	ntfs_fhtovp,
	ntfs_vptofh,
	ntfs_init,
	ntfs_sysctl,
	ntfs_checkexp,
};
