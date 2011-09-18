/*	$OpenBSD: ufs_vnops.c,v 1.102 2011/09/18 23:20:28 bluhm Exp $	*/
/*	$NetBSD: ufs_vnops.c,v 1.18 1996/05/11 18:28:04 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ufs_vnops.c	8.14 (Berkeley) 10/26/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/dirent.h>
#include <sys/lockf.h>
#include <sys/event.h>
#include <sys/poll.h>
#include <sys/specdev.h>

#include <uvm/uvm_extern.h>

#include <miscfs/fifofs/fifo.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#ifdef UFS_DIRHASH
#include <ufs/ufs/dirhash.h>
#endif
#include <ufs/ext2fs/ext2fs_extern.h>

int ufs_chmod(struct vnode *, int, struct ucred *, struct proc *);
int ufs_chown(struct vnode *, uid_t, gid_t, struct ucred *, struct proc *);
int filt_ufsread(struct knote *, long);
int filt_ufswrite(struct knote *, long);
int filt_ufsvnode(struct knote *, long);
void filt_ufsdetach(struct knote *);

union _qcvt {
	int64_t	qcvt;
	int32_t val[2];
};

#define SETHIGH(q, h) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_HIGHWORD] = (h); \
	(q) = tmp.qcvt; \
}
#define SETLOW(q, l) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_LOWWORD] = (l); \
	(q) = tmp.qcvt; \
}

/*
 * A virgin directory (no blushing please).
 */
static struct dirtemplate mastertemplate = {
	0, 12, DT_DIR, 1, ".",
	0, DIRBLKSIZ - 12, DT_DIR, 2, ".."
};
static struct odirtemplate omastertemplate = {
	0, 12, 1, ".",
	0, DIRBLKSIZ - 12, 2, ".."
};

/*
 * Create a regular file
 */
int
ufs_create(void *v)
{
	struct vop_create_args *ap = v;
	int error;

	error =
	    ufs_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
			  ap->a_dvp, ap->a_vpp, ap->a_cnp);
	if (error)
		return (error);
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	return (0);
}

/*
 * Mknod vnode call
 */
int
ufs_mknod(void *v)
{
	struct vop_mknod_args *ap = v;
	struct vattr *vap = ap->a_vap;
        struct vnode **vpp = ap->a_vpp;
	struct inode *ip;
	int error;

	if ((error =
	     ufs_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
			   ap->a_dvp, vpp, ap->a_cnp)) != 0)
		return (error);
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	ip = VTOI(*vpp);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		DIP_ASSIGN(ip, rdev, vap->va_rdev);
	}
	/*
	 * Remove inode so that it will be reloaded by VFS_VGET and
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.
	 */
	vput(*vpp);
	(*vpp)->v_type = VNON;
	vgone(*vpp);
	*vpp = NULL;
	return (0);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
int
ufs_open(void *v)
{
	struct vop_open_args *ap = v;
	struct inode *ip = VTOI(ap->a_vp);

	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((DIP(ip, flags) & APPEND) &&
	    (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);

	if (ap->a_mode & O_TRUNC)
		ip->i_flag |= IN_CHANGE | IN_UPDATE;

	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
int
ufs_close(void *v)
{
	struct vop_close_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	if (vp->v_usecount > 1) {
		struct timeval tv;

		getmicrotime(&tv);
		ITIMES(ip, &tv, &tv);
	}
	return (0);
}

int
ufs_access(void *v)
{
	struct vop_access_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	mode_t mode = ap->a_mode;

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
			int error;
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);

			if ((error = getinoquota(ip)) != 0)
				return (error);
			break;
		case VBAD:
		case VBLK:
		case VCHR:
		case VSOCK:
		case VFIFO:
		case VNON:
			break;

		}
	}

	/* If immutable bit set, nobody gets to write it. */
	if ((mode & VWRITE) && (DIP(ip, flags) & IMMUTABLE))
		return (EPERM);

	return (vaccess(vp->v_type, DIP(ip, mode), DIP(ip, uid), DIP(ip, gid),
	    mode, ap->a_cred));
}

int
ufs_getattr(void *v)
{
	struct vop_getattr_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct vattr *vap = ap->a_vap;
	struct timeval tv;

	getmicrotime(&tv);
	ITIMES(ip, &tv, &tv);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = DIP(ip, mode) & ~IFMT;
	vap->va_nlink = ip->i_effnlink;
	vap->va_uid = DIP(ip, uid);
	vap->va_gid = DIP(ip, gid);
	vap->va_rdev = (dev_t) DIP(ip, rdev);
	vap->va_size = DIP(ip, size);
	vap->va_atime.tv_sec = DIP(ip, atime);
	vap->va_atime.tv_nsec = DIP(ip, atimensec);
	vap->va_mtime.tv_sec = DIP(ip, mtime);
	vap->va_mtime.tv_nsec = DIP(ip, mtimensec);
	vap->va_ctime.tv_sec = DIP(ip, ctime);
	vap->va_ctime.tv_nsec = DIP(ip, ctimensec);
	vap->va_flags = DIP(ip, flags);
	vap->va_gen = DIP(ip, gen);
	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = dbtob((u_quad_t) DIP(ip, blocks));
	vap->va_type = vp->v_type;
	vap->va_filerev = ip->i_modrev;
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
int
ufs_setattr(void *v)
{
	struct vop_setattr_args *ap = v;
	struct vattr *vap = ap->a_vap;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ucred *cred = ap->a_cred;
	struct proc *p = ap->a_p;
	int error;
	long hint = NOTE_ATTRIB;
	u_quad_t oldsize;

	/*
	 * Check for unsettable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		return (EINVAL);
	}
	if (vap->va_flags != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (cred->cr_uid != DIP(ip, uid) &&
		    (error = suser_ucred(cred)))
			return (error);
		if (cred->cr_uid == 0) {
			if ((DIP(ip, flags) & (SF_IMMUTABLE | SF_APPEND)) &&
			    securelevel > 0)
				return (EPERM);
			DIP_ASSIGN(ip, flags, vap->va_flags);
		} else {
			if (DIP(ip, flags) & (SF_IMMUTABLE | SF_APPEND) ||
			    (vap->va_flags & UF_SETTABLE) != vap->va_flags)
				return (EPERM);
			DIP_AND(ip, flags, SF_SETTABLE);
			DIP_OR(ip, flags, vap->va_flags & UF_SETTABLE);
		}
		ip->i_flag |= IN_CHANGE;
		if (vap->va_flags & (IMMUTABLE | APPEND))
			return (0);
	}
	if (DIP(ip, flags) & (IMMUTABLE | APPEND))
		return (EPERM);
	/*
	 * Go through the fields and update if not VNOVAL.
	 */
	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		error = ufs_chown(vp, vap->va_uid, vap->va_gid, cred, p);
		if (error)
			return (error);
	}
	if (vap->va_size != VNOVAL) {
		oldsize = DIP(ip, size);
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket, fifo, or a block or
		 * character device resident on the file system.
		 */
		switch (vp->v_type) {
		case VDIR:
 			return (EISDIR);
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		default:
			break;
		}
 		if ((error = UFS_TRUNCATE(ip, vap->va_size, 0, cred)) != 0)
 			return (error);
		if (vap->va_size < oldsize)
			hint |= NOTE_TRUNCATE;
	}
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (cred->cr_uid != DIP(ip, uid) &&
		    (error = suser_ucred(cred)) &&
		    ((vap->va_vaflags & VA_UTIMES_NULL) == 0 || 
		    (error = VOP_ACCESS(vp, VWRITE, cred, p))))
			return (error);
		if (vap->va_mtime.tv_sec != VNOVAL)
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (vap->va_atime.tv_sec != VNOVAL) {
			if (!(vp->v_mount->mnt_flag & MNT_NOATIME) ||
			    (ip->i_flag & (IN_CHANGE | IN_UPDATE)))
				ip->i_flag |= IN_ACCESS;
		}
		error = UFS_UPDATE2(ip, &vap->va_atime, &vap->va_mtime, 0);
		if (error)
			return (error);
	}
	error = 0;
	if (vap->va_mode != (mode_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		error = ufs_chmod(vp, (int)vap->va_mode, cred, p);
	}
	VN_KNOTE(vp, hint);
	return (error);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
int
ufs_chmod(struct vnode *vp, int mode, struct ucred *cred, struct proc *p)
{
	struct inode *ip = VTOI(vp);
	int error;

	if (cred->cr_uid != DIP(ip, uid) &&
	    (error = suser_ucred(cred)))
		return (error);
	if (cred->cr_uid) {
		if (vp->v_type != VDIR && (mode & S_ISTXT))
			return (EFTYPE);
		if (!groupmember(DIP(ip, gid), cred) && (mode & ISGID))
			return (EPERM);
	}
	DIP_AND(ip, mode, ~ALLPERMS);
	DIP_OR(ip, mode, mode & ALLPERMS);
	ip->i_flag |= IN_CHANGE;
	if ((vp->v_flag & VTEXT) && (DIP(ip, mode) & S_ISTXT) == 0)
		(void) uvm_vnp_uncache(vp);
	return (0);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
int
ufs_chown(struct vnode *vp, uid_t uid, gid_t gid, struct ucred *cred,
    struct proc *p)
{
	struct inode *ip = VTOI(vp);
	uid_t ouid;
	gid_t ogid;
	int error = 0;
	daddr64_t change;
	enum ufs_quota_flags quota_flags = 0;

	if (uid == (uid_t)VNOVAL)
		uid = DIP(ip, uid);
	if (gid == (gid_t)VNOVAL)
		gid = DIP(ip, gid);
	/*
	 * If we don't own the file, are trying to change the owner
	 * of the file, or are not a member of the target group,
	 * the caller must be superuser or the call fails.
	 */
	if ((cred->cr_uid != DIP(ip, uid) || uid != DIP(ip, uid) ||
	    (gid != DIP(ip, gid) && !groupmember((gid_t)gid, cred))) &&
	    (error = suser_ucred(cred)))
		return (error);
	ogid = DIP(ip, gid);
	ouid = DIP(ip, uid);
	change = DIP(ip, blocks);

	if (ouid == uid)
		quota_flags |= UFS_QUOTA_NOUID;
	
	if (ogid == gid)
		quota_flags |= UFS_QUOTA_NOGID;

	if ((error = getinoquota(ip)) != 0)
		return (error);
	(void) ufs_quota_free_blocks2(ip, change, cred, quota_flags);
	(void) ufs_quota_free_inode2(ip, cred, quota_flags);
	(void) ufs_quota_delete(ip);

	DIP_ASSIGN(ip, gid, gid);
	DIP_ASSIGN(ip, uid, uid);

	if ((error = getinoquota(ip)) != 0)
		goto error;

	if ((error = ufs_quota_alloc_blocks2(ip, change, cred, 
		 quota_flags)) != 0) 
		goto error;

	if ((error = ufs_quota_alloc_inode2(ip, cred ,
		 quota_flags)) != 0) {
		(void)ufs_quota_free_blocks2(ip, change, cred, 
		    quota_flags);		
		goto error;
	}

	if (getinoquota(ip))
		panic("chown: lost quota");

	if (ouid != uid || ogid != gid)
		ip->i_flag |= IN_CHANGE;
	if (ouid != uid && cred->cr_uid != 0)
		DIP_AND(ip, mode, ~ISUID);
	if (ogid != gid && cred->cr_uid != 0)
		DIP_AND(ip, mode, ~ISGID);
	return (0);

error:
	(void) ufs_quota_delete(ip);

	DIP_ASSIGN(ip, gid, ogid);
	DIP_ASSIGN(ip, uid, ouid);

	if (getinoquota(ip) == 0) {
		(void) ufs_quota_alloc_blocks2(ip, change, cred, 
		    quota_flags | UFS_QUOTA_FORCE);
		(void) ufs_quota_alloc_inode2(ip, cred,
		    quota_flags | UFS_QUOTA_FORCE);
		(void) getinoquota(ip);
	}
	return (error);

}

/* ARGSUSED */
int
ufs_ioctl(void *v)
{
#if 0
	struct vop_ioctl_args *ap = v;
#endif
	return (ENOTTY);
}

int
ufs_poll(void *v)
{
	struct vop_poll_args *ap = v;

	/*
	 * We should really check to see if I/O is possible.
	 */
	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

int
ufs_remove(void *v)
{
	struct vop_remove_args *ap = v;
	struct inode *ip;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VDIR || (DIP(ip, flags) & (IMMUTABLE | APPEND)) ||
	    (DIP(VTOI(dvp), flags) & APPEND)) {
		error = EPERM;
		goto out;
	}
	error = ufs_dirremove(dvp, ip, ap->a_cnp->cn_flags, 0);
	VN_KNOTE(vp, NOTE_DELETE);
	VN_KNOTE(dvp, NOTE_WRITE);
 out:
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);
	return (error);
}

/*
 * link vnode call
 */
int
ufs_link(void *v)
{
	struct vop_link_args *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	struct componentname *cnp = ap->a_cnp;
	struct proc *p = cnp->cn_proc;
	struct inode *ip;
	struct direct newdir;
	int error;

#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_link: no name");
#endif
	if (vp->v_type == VDIR) {
		VOP_ABORTOP(dvp, cnp);
		error = EPERM;
		goto out2;
	}
	if (dvp->v_mount != vp->v_mount) {
		VOP_ABORTOP(dvp, cnp);
		error = EXDEV;
		goto out2;
	}
	if (dvp != vp && (error = vn_lock(vp, LK_EXCLUSIVE, p))) {
		VOP_ABORTOP(dvp, cnp);
		goto out2;
	}
	ip = VTOI(vp);
	if ((nlink_t) DIP(ip, nlink) >= LINK_MAX) {
		VOP_ABORTOP(dvp, cnp);
		error = EMLINK;
		goto out1;
	}
	if (DIP(ip, flags) & (IMMUTABLE | APPEND)) {
		VOP_ABORTOP(dvp, cnp);
		error = EPERM;
		goto out1;
	}
	ip->i_effnlink++;
	DIP_ADD(ip, nlink, 1);
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(vp))
		softdep_change_linkcnt(ip, 0);
	if ((error = UFS_UPDATE(ip, !DOINGSOFTDEP(vp))) == 0) {
		ufs_makedirentry(ip, cnp, &newdir);
		error = ufs_direnter(dvp, vp, &newdir, cnp, NULL);
	}
	if (error) {
		ip->i_effnlink--;
		DIP_ADD(ip, nlink, -1);
		ip->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(vp))
			softdep_change_linkcnt(ip, 0);
	}
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);
out1:
	if (dvp != vp)
		VOP_UNLOCK(vp, 0, p);
out2:
	vput(dvp);
	return (error);
}

/*
 * Rename system call.
 * 	rename("foo", "bar");
 * is essentially
 *	unlink("bar");
 *	link("foo", "bar");
 *	unlink("foo");
 * but ``atomically''.  Can't do full commit without saving state in the
 * inode on disk which isn't feasible at this time.  Best we can do is
 * always guarantee the target exists.
 *
 * Basic algorithm is:
 *
 * 1) Bump link count on source while we're linking it to the
 *    target.  This also ensure the inode won't be deleted out
 *    from underneath us while we work (it may be truncated by
 *    a concurrent `trunc' or `open' for creation).
 * 2) Link source to destination.  If destination already exists,
 *    delete it first.
 * 3) Unlink source reference to inode if still around. If a
 *    directory was moved and the parent of the destination
 *    is different from the source, patch the ".." entry in the
 *    directory.
 */
int
ufs_rename(void *v)
{
	struct vop_rename_args *ap = v;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct proc *p = fcnp->cn_proc;
	struct inode *ip, *xp, *dp;
	struct direct newdir;
	int doingdirectory = 0, oldparent = 0, newparent = 0;
	int error = 0;

#ifdef DIAGNOSTIC
	if ((tcnp->cn_flags & HASBUF) == 0 ||
	    (fcnp->cn_flags & HASBUF) == 0)
		panic("ufs_rename: no name");
#endif
	/*
	 * Check for cross-device rename.
	 */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
abortit:
		VOP_ABORTOP(tdvp, tcnp);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fdvp, fcnp);
		vrele(fdvp);
		vrele(fvp);
		return (error);
	}

	if (tvp && ((DIP(VTOI(tvp), flags) & (IMMUTABLE | APPEND)) ||
	    (DIP(VTOI(tdvp), flags) & APPEND))) {
		error = EPERM;
		goto abortit;
	}

	/*
	 * Check if just deleting a link name or if we've lost a race.
	 * If another process completes the same rename after we've looked
	 * up the source and have blocked looking up the target, then the
	 * source and target inodes may be identical now although the
	 * names were never linked.
	 */
	if (fvp == tvp) {
		if (fvp->v_type == VDIR) {
			/*
			 * Linked directories are impossible, so we must
			 * have lost the race.  Pretend that the rename
			 * completed before the lookup.
			 */
			error = ENOENT;
			goto abortit;
		}

		/* Release destination completely. */
		VOP_ABORTOP(tdvp, tcnp);
		vput(tdvp);
		vput(tvp);

		/*
		 * Delete source.  There is another race now that everything
		 * is unlocked, but this doesn't cause any new complications.
		 * relookup() may find a file that is unrelated to the
		 * original one, or it may fail.  Too bad.
		 */
		vrele(fvp);
		fcnp->cn_flags &= ~MODMASK;
		fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
		if ((fcnp->cn_flags & SAVESTART) == 0)
			panic("ufs_rename: lost from startdir");
		fcnp->cn_nameiop = DELETE;
		if ((error = vfs_relookup(fdvp, &fvp, fcnp)) != 0)
			return (error);		/* relookup did vrele() */
		vrele(fdvp);
		return (VOP_REMOVE(fdvp, fvp, fcnp));
	}

	if ((error = vn_lock(fvp, LK_EXCLUSIVE, p)) != 0)
		goto abortit;

	/* fvp, tdvp, tvp now locked */
	dp = VTOI(fdvp);
	ip = VTOI(fvp);
	if ((nlink_t) DIP(ip, nlink) >= LINK_MAX) {
		VOP_UNLOCK(fvp, 0, p);
		error = EMLINK;
		goto abortit;
	}
	if ((DIP(ip, flags) & (IMMUTABLE | APPEND)) ||
	    (DIP(dp, flags) & APPEND)) {
		VOP_UNLOCK(fvp, 0, p);
		error = EPERM;
		goto abortit;
	}
	if ((DIP(ip, mode) & IFMT) == IFDIR) {
		error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred, tcnp->cn_proc);
		if (!error && tvp)
			error = VOP_ACCESS(tvp, VWRITE, tcnp->cn_cred, tcnp->cn_proc);
		if (error) {
			VOP_UNLOCK(fvp, 0, p);
			error = EACCES;
			goto abortit;
		}
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') ||
		    dp == ip ||
		    (fcnp->cn_flags & ISDOTDOT) ||
		    (tcnp->cn_flags & ISDOTDOT) ||
		    (ip->i_flag & IN_RENAME)) {
			VOP_UNLOCK(fvp, 0, p);
			error = EINVAL;
			goto abortit;
		}
		ip->i_flag |= IN_RENAME;
		oldparent = dp->i_number;
		doingdirectory = 1;
	}
	VN_KNOTE(fdvp, NOTE_WRITE);		/* XXX right place? */

	/*
	 * When the target exists, both the directory
	 * and target vnodes are returned locked.
	 */
	dp = VTOI(tdvp);
	xp = NULL;
	if (tvp)
		xp = VTOI(tvp);

	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work, the link count
	 *    may be wrong, but correctable.
	 */
	ip->i_effnlink++;
	DIP_ADD(ip, nlink, 1);
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(fvp))
		softdep_change_linkcnt(ip, 0);
	if ((error = UFS_UPDATE(ip, !DOINGSOFTDEP(fvp))) != 0) {
		VOP_UNLOCK(fvp, 0, p);
		goto bad;
	}

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory hierarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..". We must repeat the call 
	 * to namei, as the parent directory is unlocked by the
	 * call to checkpath().
	 */
	error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred, tcnp->cn_proc);
	VOP_UNLOCK(fvp, 0, p);

	/* tdvp and tvp locked */
	if (oldparent != dp->i_number)
		newparent = dp->i_number;
	if (doingdirectory && newparent) {
		if (error)	/* write access check above */
			goto bad;
		if (xp != NULL)
			vput(tvp);
		/*
		 * Compensate for the reference ufs_checkpath() loses.
		 */
		vref(tdvp);
		/* Only tdvp is locked */
		if ((error = ufs_checkpath(ip, dp, tcnp->cn_cred)) != 0) {
			vrele(tdvp);
			goto out;
		}
		if ((tcnp->cn_flags & SAVESTART) == 0)
			panic("ufs_rename: lost to startdir");
		if ((error = vfs_relookup(tdvp, &tvp, tcnp)) != 0)
			goto out;
		vrele(tdvp); /* relookup() acquired a reference */
		dp = VTOI(tdvp);
		xp = NULL;
		if (tvp)
			xp = VTOI(tvp);
	}
	/*
	 * 2) If target doesn't exist, link the target
	 *    to the source and unlink the source. 
	 *    Otherwise, rewrite the target directory
	 *    entry to reference the source inode and
	 *    expunge the original entry's existence.
	 */
	if (xp == NULL) {
		if (dp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Account for ".." in new directory.
		 * When source and destination have the same
		 * parent we don't fool with the link count.
		 */
		if (doingdirectory && newparent) {
			if ((nlink_t) DIP(dp, nlink) >= LINK_MAX) {
				error = EMLINK;
				goto bad;
			}
			dp->i_effnlink++;
			DIP_ADD(dp, nlink, 1);
			dp->i_flag |= IN_CHANGE;
			if (DOINGSOFTDEP(tdvp))
                               softdep_change_linkcnt(dp, 0);
			if ((error = UFS_UPDATE(dp, !DOINGSOFTDEP(tdvp))) 
			    != 0) {
				dp->i_effnlink--;
				DIP_ADD(dp, nlink, -1);
				dp->i_flag |= IN_CHANGE;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(dp, 0);
				goto bad;
			}
		}
		ufs_makedirentry(ip, tcnp, &newdir);
		if ((error = ufs_direnter(tdvp, NULL, &newdir, tcnp, NULL)) != 0) {
			if (doingdirectory && newparent) {
				dp->i_effnlink--;
				DIP_ADD(dp, nlink, -1);
				dp->i_flag |= IN_CHANGE;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(dp, 0);
				(void)UFS_UPDATE(dp, 1);
			}
			goto bad;
		}
		VN_KNOTE(tdvp, NOTE_WRITE);
		vput(tdvp);
	} else {
		if (xp->i_dev != dp->i_dev || xp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Short circuit rename(foo, foo).
		 */
		if (xp->i_number == ip->i_number)
			panic("ufs_rename: same file");
		/*
		 * If the parent directory is "sticky", then the user must
		 * own the parent directory, or the destination of the rename,
		 * otherwise the destination may not be changed (except by
		 * root). This implements append-only directories.
		 */
		if ((DIP(dp, mode) & S_ISTXT) && tcnp->cn_cred->cr_uid != 0 &&
		    tcnp->cn_cred->cr_uid != DIP(dp, uid) &&
		    DIP(xp, uid )!= tcnp->cn_cred->cr_uid) {
			error = EPERM;
			goto bad;
		}
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if ((DIP(xp, mode) & IFMT) == IFDIR) {
			if (xp->i_effnlink > 2 ||
			    !ufs_dirempty(xp, dp->i_number, tcnp->cn_cred)) {
				error = ENOTEMPTY;
				goto bad;
			}
			if (!doingdirectory) {
				error = ENOTDIR;
				goto bad;
			}
			cache_purge(tdvp);
		} else if (doingdirectory) {
			error = EISDIR;
			goto bad;
		}
		
		if ((error = ufs_dirrewrite(dp, xp, ip->i_number,
                   IFTODT(DIP(ip, mode)), (doingdirectory && newparent) ?
		   newparent : doingdirectory)) != 0)
                        goto bad;
		if (doingdirectory) {
			if (!newparent) {
				dp->i_effnlink--;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(dp, 0);
			}
			xp->i_effnlink--;
			if (DOINGSOFTDEP(tvp))
				softdep_change_linkcnt(xp, 0);
		}
		if (doingdirectory && !DOINGSOFTDEP(tvp)) {
		       /*
			* Truncate inode. The only stuff left in the directory
			* is "." and "..". The "." reference is inconsequential
                        * since we are quashing it. We have removed the "."
                        * reference and the reference in the parent directory,
                        * but there may be other hard links. The soft
                        * dependency code will arrange to do these operations
                        * after the parent directory entry has been deleted on
                        * disk, so when running with that code we avoid doing
                        * them now.
                        */
			if (!newparent) {
				DIP_ADD(dp, nlink, -1);
				dp->i_flag |= IN_CHANGE;
			}

			DIP_ADD(xp, nlink, -1);
			xp->i_flag |= IN_CHANGE;
			if ((error = UFS_TRUNCATE(VTOI(tvp), (off_t)0, IO_SYNC,
			        tcnp->cn_cred)) != 0)
				goto bad;
                }
		VN_KNOTE(tdvp, NOTE_WRITE);
	        vput(tdvp);
		VN_KNOTE(tvp, NOTE_DELETE);
		vput(tvp);
		xp = NULL;
	}

	/*
	 * 3) Unlink the source.
	 */
	fcnp->cn_flags &= ~MODMASK;
	fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
	if ((fcnp->cn_flags & SAVESTART) == 0)
		panic("ufs_rename: lost from startdir");
	if ((error = vfs_relookup(fdvp, &fvp, fcnp)) != 0) {
		vrele(ap->a_fvp);
		return (error);
	}
	vrele(fdvp);
	if (fvp == NULL) {
		/*
		 * From name has disappeared.
		 */
		if (doingdirectory)
			panic("ufs_rename: lost dir entry");
		vrele(ap->a_fvp);
		return (0);
	}

	xp = VTOI(fvp);
	dp = VTOI(fdvp);

	/*
	 * Ensure that the directory entry still exists and has not
	 * changed while the new name has been entered. If the source is
	 * a file then the entry may have been unlinked or renamed. In
	 * either case there is no further work to be done. If the source
	 * is a directory then it cannot have been rmdir'ed; the IN_RENAME 
	 * flag ensures that it cannot be moved by another rename or removed
	 * by a rmdir.
	 */
	if (xp != ip) {
		if (doingdirectory)
			panic("ufs_rename: lost dir entry");
	} else {
		/*
		 * If the source is a directory with a
		 * new parent, the link count of the old
		 * parent directory must be decremented
		 * and ".." set to point to the new parent.
		 */
		if (doingdirectory && newparent) {
			xp->i_offset = mastertemplate.dot_reclen;
			ufs_dirrewrite(xp, dp, newparent, DT_DIR, 0);
			cache_purge(fdvp);
		}
		error = ufs_dirremove(fdvp, xp, fcnp->cn_flags, 0);
		xp->i_flag &= ~IN_RENAME;
	}
	VN_KNOTE(fvp, NOTE_RENAME);
	if (dp)
		vput(fdvp);
	if (xp)
		vput(fvp);
	vrele(ap->a_fvp);
	return (error);

bad:
	if (xp)
		vput(ITOV(xp));
	vput(ITOV(dp));
out:
	vrele(fdvp);
	if (doingdirectory)
		ip->i_flag &= ~IN_RENAME;
	if (vn_lock(fvp, LK_EXCLUSIVE, p) == 0) {
		ip->i_effnlink--;
		DIP_ADD(ip, nlink, -1);
		ip->i_flag |= IN_CHANGE;
		ip->i_flag &= ~IN_RENAME;
		if (DOINGSOFTDEP(fvp))
			softdep_change_linkcnt(ip, 0);
		vput(fvp);
	} else
		vrele(fvp);
	return (error);
}

/*
 * Mkdir system call
 */
int
ufs_mkdir(void *v)
{
	struct vop_mkdir_args *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	struct vnode *tvp;
	struct buf *bp;
	struct direct newdir;
	struct dirtemplate dirtemplate, *dtp;
	int error, dmode, blkoff;

#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_mkdir: no name");
#endif
	dp = VTOI(dvp);
	if ((nlink_t) DIP(dp, nlink) >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	dmode = vap->va_mode & 0777;
	dmode |= IFDIR;
	/*
	 * Must simulate part of ufs_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	if ((error = UFS_INODE_ALLOC(dp, dmode, cnp->cn_cred, &tvp)) != 0)
		goto out;

	ip = VTOI(tvp);

	DIP_ASSIGN(ip, uid, cnp->cn_cred->cr_uid);
	DIP_ASSIGN(ip, gid, DIP(dp, gid));

	if ((error = getinoquota(ip)) ||
	    (error = ufs_quota_alloc_inode(ip, cnp->cn_cred))) {
		pool_put(&namei_pool, cnp->cn_pnbuf);
		UFS_INODE_FREE(ip, ip->i_number, dmode);
		vput(tvp);
		vput(dvp);
		return (error);
	}

	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	DIP_ASSIGN(ip, mode, dmode);
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 2;
	DIP_ASSIGN(ip, nlink, 2);
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip, 0);

	/*
	 * Bump link count in parent directory to reflect work done below.
	 * Should be done before reference is create so cleanup is 
	 * possible if we crash.
	 */
	dp->i_effnlink++;
	DIP_ADD(dp, nlink, 1);
	dp->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(dvp))
		softdep_change_linkcnt(dp, 0);
	if ((error = UFS_UPDATE(dp, !DOINGSOFTDEP(dvp))) != 0)
		goto bad;

	/* 
	 * Initialize directory with "." and ".." from static template.
	 */
	if (dvp->v_mount->mnt_maxsymlinklen > 0)
		dtp = &mastertemplate;
	else
		dtp = (struct dirtemplate *)&omastertemplate;
	dirtemplate = *dtp;
	dirtemplate.dot_ino = ip->i_number;
	dirtemplate.dotdot_ino = dp->i_number;

	if ((error = UFS_BUF_ALLOC(ip, (off_t)0, DIRBLKSIZ, cnp->cn_cred,
            B_CLRBUF, &bp)) != 0)
		goto bad;
	DIP_ASSIGN(ip, size, DIRBLKSIZ);
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	uvm_vnp_setsize(tvp, DIP(ip, size));
	bcopy((caddr_t)&dirtemplate, (caddr_t)bp->b_data, sizeof dirtemplate);
	if (DOINGSOFTDEP(tvp)) {
		/*
		 * Ensure that the entire newly allocated block is a
		 * valid directory so that future growth within the
		 * block does not have to ensure that the block is
		 * written before the inode
		 */
		blkoff = DIRBLKSIZ;
		while (blkoff < bp->b_bcount) {
			((struct direct *)
			 (bp->b_data + blkoff))->d_reclen = DIRBLKSIZ;
			blkoff += DIRBLKSIZ;
		}
	}
	if ((error = UFS_UPDATE(ip, !DOINGSOFTDEP(tvp))) != 0) {
		(void)VOP_BWRITE(bp);
		goto bad;
	}

	/*
         * Directory set up, now install its entry in the parent directory.
         *
         * If we are not doing soft dependencies, then we must write out the
         * buffer containing the new directory body before entering the new
         * name in the parent. If we are doing soft dependencies, then the
         * buffer containing the new directory body will be passed to and
         * released in the soft dependency code after the code has attached
         * an appropriate ordering dependency to the buffer which ensures that
         * the buffer is written before the new name is written in the parent.
	 */
        if (!DOINGSOFTDEP(dvp) && ((error = VOP_BWRITE(bp)) != 0))
                goto bad;
        ufs_makedirentry(ip, cnp, &newdir);
        error = ufs_direnter(dvp, tvp, &newdir, cnp, bp);
  
bad:
        if (error == 0) {
		VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
                *ap->a_vpp = tvp;
        } else {
                dp->i_effnlink--;
                DIP_ADD(dp, nlink, -1);
                dp->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(dvp))
			softdep_change_linkcnt(dp, 0);
                /*
                 * No need to do an explicit VOP_TRUNCATE here, vrele will
                 * do this for us because we set the link count to 0.
                 */
                ip->i_effnlink = 0;
                DIP_ASSIGN(ip, nlink, 0);
                ip->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(tvp))
			softdep_change_linkcnt(ip, 0);
		vput(tvp);
	}
out:
	pool_put(&namei_pool, cnp->cn_pnbuf);
	vput(dvp);

	return (error);
}

/*
 * Rmdir system call.
 */
int
ufs_rmdir(void *v)
{
	struct vop_rmdir_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	int error;

	ip = VTOI(vp);
	dp = VTOI(dvp);
	/*
	 * No rmdir "." or of mounted on directories.
	 */
	if (dp == ip || vp->v_mountedhere != NULL) {
		if (dp == ip)
			vrele(dvp);
		else
			vput(dvp);
		vput(vp);
		return (EINVAL);
	}
	/*
         * Do not remove a directory that is in the process of being renamed.
         * Verify the directory is empty (and valid). Rmdir ".." will not be
         * valid since ".." will contain a reference to the current directory
         * and thus be non-empty.
	 */
	error = 0;
	if (ip->i_flag & IN_RENAME) {
		error = EINVAL;
		goto out;
	}
	if (ip->i_effnlink != 2 ||
	    !ufs_dirempty(ip, dp->i_number, cnp->cn_cred)) {
		error = ENOTEMPTY;
		goto out;
	}
	if ((DIP(dp, flags) & APPEND) ||
		(DIP(ip, flags) & (IMMUTABLE | APPEND))) {
		error = EPERM;
		goto out;
	}
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	dp->i_effnlink--;
	ip->i_effnlink--;
	if (DOINGSOFTDEP(vp)) {
		softdep_change_linkcnt(dp, 0);
		softdep_change_linkcnt(ip, 0);
	}
	if ((error = ufs_dirremove(dvp, ip, cnp->cn_flags, 1)) != 0) {
		dp->i_effnlink++;
		ip->i_effnlink++;
		if (DOINGSOFTDEP(vp)) {
			softdep_change_linkcnt(dp, 0);
			softdep_change_linkcnt(ip, 0);
		}
		goto out;
	}

	VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
	cache_purge(dvp);
        /*
	 * Truncate inode. The only stuff left in the directory is "." and
	 * "..". The "." reference is inconsequential since we are quashing
	 * it. The soft dependency code will arrange to do these operations
	 * after the parent directory entry has been deleted on disk, so
	 * when running with that code we avoid doing them now.
	 */
	if (!DOINGSOFTDEP(vp)) {
		int ioflag;

		DIP_ADD(dp, nlink, -1);
		dp->i_flag |= IN_CHANGE;
		DIP_ADD(ip, nlink, -1);
		ip->i_flag |= IN_CHANGE;
		ioflag = DOINGASYNC(vp) ? 0 : IO_SYNC;
		error = UFS_TRUNCATE(ip, (off_t)0, ioflag, cnp->cn_cred);
	}
	cache_purge(vp);
#ifdef UFS_DIRHASH
	/* Kill any active hash; i_effnlink == 0, so it will not come back. */
	if (ip->i_dirhash != NULL)
		ufsdirhash_free(ip);
#endif

out:
	VN_KNOTE(vp, NOTE_DELETE);
        vput(dvp);
	vput(vp);
	return (error);
}

/*
 * symlink -- make a symbolic link
 */
int
ufs_symlink(void *v)
{
	struct vop_symlink_args *ap = v;
	struct vnode *vp, **vpp = ap->a_vpp;
	struct inode *ip;
	int len, error;

	error = ufs_makeinode(IFLNK | ap->a_vap->va_mode, ap->a_dvp,
			      vpp, ap->a_cnp);
	if (error)
		return (error);
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	vp = *vpp;
	len = strlen(ap->a_target);
	if (len < vp->v_mount->mnt_maxsymlinklen) {
		ip = VTOI(vp);
		bcopy(ap->a_target, (char *)SHORTLINK(ip), len);
		DIP_ASSIGN(ip, size, len);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	} else
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED, ap->a_cnp->cn_cred, NULL,
		    curproc);
	vput(vp);
	return (error);
}

/*
 * Vnode op for reading directories.
 * 
 * The routine below assumes that the on-disk format of a directory
 * is the same as that defined by <sys/dirent.h>. If the on-disk
 * format changes, then it will be necessary to do a conversion
 * from the on-disk format that read returns to the format defined
 * by <sys/dirent.h>.
 */
int
ufs_readdir(void *v)
{
	struct vop_readdir_args *ap = v;
	struct uio *uio = ap->a_uio;
	int error;
	size_t count, lost, entries;
	off_t off = uio->uio_offset;

	count = uio->uio_resid;
	entries = (uio->uio_offset + count) & (DIRBLKSIZ - 1);

	/* Make sure we don't return partial entries. */
	if (count <= entries)
		return (EINVAL);

	count -= entries;
	lost = uio->uio_resid - count;
	uio->uio_resid = count;
	uio->uio_iov->iov_len = count;
#	if (BYTE_ORDER == LITTLE_ENDIAN)
		if (ap->a_vp->v_mount->mnt_maxsymlinklen > 0) {
			error = VOP_READ(ap->a_vp, uio, 0, ap->a_cred);
		} else {
			struct dirent *dp, *edp;
			struct uio auio;
			struct iovec aiov;
			caddr_t dirbuf;
			int readcnt;
			u_char tmp;

			auio = *uio;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_segflg = UIO_SYSSPACE;
			aiov.iov_len = count;
			dirbuf = malloc(count, M_TEMP, M_WAITOK);
			aiov.iov_base = dirbuf;
			error = VOP_READ(ap->a_vp, &auio, 0, ap->a_cred);
			if (error == 0) {
				readcnt = count - auio.uio_resid;
				edp = (struct dirent *)&dirbuf[readcnt];
				for (dp = (struct dirent *)dirbuf; dp < edp; ) {
					tmp = dp->d_namlen;
					dp->d_namlen = dp->d_type;
					dp->d_type = tmp;
					if (dp->d_reclen > 0) {
						dp = (struct dirent *)
						    ((char *)dp + dp->d_reclen);
					} else {
						error = EIO;
						break;
					}
				}
				if (dp >= edp)
					error = uiomove(dirbuf, readcnt, uio);
			}
			free(dirbuf, M_TEMP);
		}
#	else
		error = VOP_READ(ap->a_vp, uio, 0, ap->a_cred);
#	endif
	if (!error && ap->a_ncookies) {
		struct dirent *dp, *dpstart;
		off_t offstart;
		u_long *cookies;
		int ncookies;

		/*
		 * Only the NFS server and emulations use cookies, and they
		 * load the directory block into system space, so we can
		 * just look at it directly.
		 */
		if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
			panic("ufs_readdir: lost in space");

		dpstart = (struct dirent *)
			((char *)uio->uio_iov->iov_base -
			(uio->uio_offset - off));
                offstart = off;
                for (dp = dpstart, ncookies = 0; off < uio->uio_offset; ) {
                        if (dp->d_reclen == 0)
                                break;
                        off += dp->d_reclen;
                        ncookies++;
                        dp = (struct dirent *)((caddr_t)dp + dp->d_reclen);
                }
                lost += uio->uio_offset - off;
                uio->uio_offset = off;
                cookies = malloc(ncookies * sizeof(u_long), M_TEMP, M_WAITOK);
                *ap->a_ncookies = ncookies;
                *ap->a_cookies = cookies;
                for (off = offstart, dp = dpstart; off < uio->uio_offset; ) {
			off += dp->d_reclen;
                        *cookies = off;
			cookies++;
                        dp = (struct dirent *)((caddr_t)dp + dp->d_reclen);
		}
	}

	uio->uio_resid += lost;
	*ap->a_eofflag = DIP(VTOI(ap->a_vp), size) <= uio->uio_offset;

	return (error);
}

/*
 * Return target name of a symbolic link
 */
int
ufs_readlink(void *v)
{
	struct vop_readlink_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	int isize;

	isize = DIP(ip, size);
	if (isize < vp->v_mount->mnt_maxsymlinklen ||
	    (vp->v_mount->mnt_maxsymlinklen == 0 && DIP(ip, blocks) == 0)) {
		return (uiomove((char *)SHORTLINK(ip), isize, ap->a_uio));
	}
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Lock an inode. If its already locked, set the WANT bit and sleep.
 */
int
ufs_lock(void *v)
{
	struct vop_lock_args *ap = v;
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTOI(vp)->i_lock, ap->a_flags, NULL));
}

/*
 * Unlock an inode.  If WANT bit is on, wakeup.
 */
int
ufs_unlock(void *v)
{
	struct vop_unlock_args *ap = v;
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTOI(vp)->i_lock, ap->a_flags | LK_RELEASE, NULL));
}

/*
 * Check for a locked inode.
 */
int
ufs_islocked(void *v)
{
	struct vop_islocked_args *ap = v;

	return (lockstatus(&VTOI(ap->a_vp)->i_lock));
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ufs_strategy(void *v)
{
	struct vop_strategy_args *ap = v;
	struct buf *bp = ap->a_bp;
	struct vnode *vp = bp->b_vp;
	struct inode *ip;
	int error;
	int s;

	ip = VTOI(vp);
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("ufs_strategy: spec");
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno,
				 NULL);
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			s = splbio();
			biodone(bp);
			splx(s);
			return (error);
		}
		if (bp->b_blkno == -1)
			clrbuf(bp);
	}
	if (bp->b_blkno == -1) {
		s = splbio();
		biodone(bp);
		splx(s);
		return (0);
	}
	vp = ip->i_devvp;
	bp->b_dev = vp->v_rdev;
	(vp->v_op->vop_strategy)(ap);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
int
ufs_print(void *v)
{
#ifdef DIAGNOSTIC
	struct vop_print_args *ap = v;

	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	printf("tag VT_UFS, ino %u, on dev %d, %d", ip->i_number,
		major(ip->i_dev), minor(ip->i_dev));
	printf(" flags 0x%x, effnlink %d, nlink %d\n",
	       ip->i_flag, ip->i_effnlink, DIP(ip, nlink));
	printf("\tmode 0%o, owner %d, group %d, size %lld",
	       DIP(ip, mode), DIP(ip, uid), DIP(ip, gid), DIP(ip, size));

#ifdef FIFO
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
#endif /* FIFO */
	lockmgr_printinfo(&ip->i_lock);
	printf("\n");

#endif /* DIAGNOSTIC */

	return (0);
}

/*
 * Read wrapper for special devices.
 */
int
ufsspec_read(void *v)
{
	struct vop_read_args *ap = v;

	/*
	 * Set access flag.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (spec_read(ap));
}

/*
 * Write wrapper for special devices.
 */
int
ufsspec_write(void *v)
{
	struct vop_write_args *ap = v;

	/*
	 * Set update and change flags.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (spec_write(ap));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the inode then do device close.
 */
int
ufsspec_close(void *v)
{
	struct vop_close_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	if (ap->a_vp->v_usecount > 1) {
		struct timeval tv;

		getmicrotime(&tv);
		ITIMES(ip, &tv, &tv);
	}
	return (spec_close(ap));
}

#ifdef FIFO
/*
 * Read wrapper for fifo's
 */
int
ufsfifo_read(void *v)
{
	struct vop_read_args *ap = v;

	/*
	 * Set access flag.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (fifo_read(ap));
}

/*
 * Write wrapper for fifo's.
 */
int
ufsfifo_write(void *v)
{
	struct vop_write_args *ap = v;

	/*
	 * Set update and change flags.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (fifo_write(ap));
}

/*
 * Close wrapper for fifo's.
 *
 * Update the times on the inode then do device close.
 */
int
ufsfifo_close(void *v)
{
	struct vop_close_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	if (ap->a_vp->v_usecount > 1) {
		struct timeval tv;

		getmicrotime(&tv);
		ITIMES(ip, &tv, &tv);
	}
	return (fifo_close(ap));
}
#endif /* FIFO */

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
int
ufs_pathconf(void *v)
{
	struct vop_pathconf_args *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Advisory record locking support
 */
int
ufs_advlock(void *v)
{
	struct vop_advlock_args *ap = v;
	struct inode *ip = VTOI(ap->a_vp);

	return (lf_advlock(&ip->i_lockf, DIP(ip, size), ap->a_id, ap->a_op,
	    ap->a_fl, ap->a_flags));
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
int
ufs_vinit(struct mount *mntp, struct vops *specops, struct vops *fifoops,
    struct vnode **vpp)
{
	struct inode *ip;
	struct vnode *vp, *nvp;
	struct timeval mtv;

	vp = *vpp;
	ip = VTOI(vp);
	switch(vp->v_type = IFTOVT(DIP(ip, mode))) {
	case VCHR:
	case VBLK:
		vp->v_op = specops;
		if ((nvp = checkalias(vp, DIP(ip, rdev), mntp)) != NULL) {
			/*
			 * Discard unneeded vnode, but save its inode.
			 * Note that the lock is carried over in the inode
			 * to the replacement vnode.
			 */
			nvp->v_data = vp->v_data;
			vp->v_data = NULL;
			vp->v_op = &spec_vops;
#ifdef VFSLCKDEBUG
			vp->v_flag &= ~VLOCKSWORK;
#endif
			vrele(vp);
			vgone(vp);
			/*
			 * Reinitialize aliased inode.
			 */
			vp = nvp;
			ip->i_vnode = vp;
		}
		break;
	case VFIFO:
#ifdef FIFO
		vp->v_op = fifoops;
		break;
#else
		return (EOPNOTSUPP);
#endif
	case VNON:
	case VBAD:
	case VSOCK:
	case VLNK:
	case VDIR:
	case VREG:
		break;
	}
	if (ip->i_number == ROOTINO)
                vp->v_flag |= VROOT;
	/*
	 * Initialize modrev times
	 */
	getmicrouptime(&mtv);
	SETHIGH(ip->i_modrev, mtv.tv_sec);
	SETLOW(ip->i_modrev, mtv.tv_usec * 4294);
	*vpp = vp;
	return (0);
}

/*
 * Allocate a new inode.
 */
int
ufs_makeinode(int mode, struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp)
{
	struct inode *ip, *pdir;
	struct direct newdir;
	struct vnode *tvp;
	int error;

	pdir = VTOI(dvp);
#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_makeinode: no name");
#endif
	*vpp = NULL;
	if ((mode & IFMT) == 0)
		mode |= IFREG;

	if ((error = UFS_INODE_ALLOC(pdir, mode, cnp->cn_cred, &tvp)) != 0) {
		pool_put(&namei_pool, cnp->cn_pnbuf);
		vput(dvp);
		return (error);
	}

	ip = VTOI(tvp);

	DIP_ASSIGN(ip, gid, DIP(pdir, gid));
	DIP_ASSIGN(ip, uid, cnp->cn_cred->cr_uid);

	if ((error = getinoquota(ip)) ||
	    (error = ufs_quota_alloc_inode(ip, cnp->cn_cred))) {
		pool_put(&namei_pool, cnp->cn_pnbuf);
		UFS_INODE_FREE(ip, ip->i_number, mode);
		vput(tvp);
		vput(dvp);
		return (error);
	}

	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	DIP_ASSIGN(ip, mode, mode);
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 1;
	DIP_ASSIGN(ip, nlink, 1);
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip, 0);
	if ((DIP(ip, mode) & ISGID) &&
		!groupmember(DIP(ip, gid), cnp->cn_cred) &&
	    suser_ucred(cnp->cn_cred))
		DIP_AND(ip, mode, ~ISGID);

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	if ((error = UFS_UPDATE(ip, !DOINGSOFTDEP(tvp))) != 0)
		goto bad;

	ufs_makedirentry(ip, cnp, &newdir);
	if ((error = ufs_direnter(dvp, tvp, &newdir, cnp, NULL)) != 0)
		goto bad;

	if ((cnp->cn_flags & SAVESTART) == 0)
		pool_put(&namei_pool, cnp->cn_pnbuf);
	vput(dvp);
	*vpp = tvp;
	return (0);

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	pool_put(&namei_pool, cnp->cn_pnbuf);
	vput(dvp);
	ip->i_effnlink = 0;
	DIP_ASSIGN(ip, nlink, 0);
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip, 0);
	tvp->v_type = VNON;
	vput(tvp);

	return (error);
}

struct filterops ufsread_filtops = 
	{ 1, NULL, filt_ufsdetach, filt_ufsread };
struct filterops ufswrite_filtops = 
	{ 1, NULL, filt_ufsdetach, filt_ufswrite };
struct filterops ufsvnode_filtops = 
	{ 1, NULL, filt_ufsdetach, filt_ufsvnode };

int
ufs_kqfilter(void *v)
{
	struct vop_kqfilter_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct knote *kn = ap->a_kn;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &ufsread_filtops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &ufswrite_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &ufsvnode_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = (caddr_t)vp;

	SLIST_INSERT_HEAD(&vp->v_selectinfo.si_note, kn, kn_selnext);

	return (0);
}

void
filt_ufsdetach(struct knote *kn)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;

	SLIST_REMOVE(&vp->v_selectinfo.si_note, kn, knote, kn_selnext);
}

int
filt_ufsread(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	struct inode *ip = VTOI(vp);

	/*
	 * filesystem is gone, so set the EOF flag and schedule 
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}

        kn->kn_data = DIP(ip, size) - kn->kn_fp->f_offset;
	if (kn->kn_data == 0 && kn->kn_sfflags & NOTE_EOF) {
		kn->kn_fflags |= NOTE_EOF;
		return (1);
	}

        return (kn->kn_data != 0);
}

int
filt_ufswrite(struct knote *kn, long hint)
{
	/*
	 * filesystem is gone, so set the EOF flag and schedule 
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}

        kn->kn_data = 0;
        return (1);
}

int
filt_ufsvnode(struct knote *kn, long hint)
{
	if (kn->kn_sfflags & hint)
		kn->kn_fflags |= hint;
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	return (kn->kn_fflags != 0);
}
