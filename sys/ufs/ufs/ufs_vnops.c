/*	$OpenBSD: ufs_vnops.c,v 1.60 2005/02/17 18:07:37 jfb Exp $	*/
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

#include <uvm/uvm_extern.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#ifdef UFS_DIRHASH
#include <ufs/ufs/dirhash.h>
#endif
#include <ufs/ext2fs/ext2fs_extern.h>

static int ufs_chmod(struct vnode *, int, struct ucred *, struct proc *);
static int ufs_chown(struct vnode *, uid_t, gid_t, struct ucred *, struct proc *);
int filt_ufsread(struct knote *kn, long hint);
int filt_ufswrite(struct knote *kn, long hint);
int filt_ufsvnode(struct knote *kn, long hint);
void filt_ufsdetach(struct knote *kn);

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
#define VN_KNOTE(vp, b) \
	KNOTE(&vp->v_selectinfo.vsi_selinfo.si_note, (b))


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
ufs_create(v)
	void *v;
{
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
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
/* ARGSUSED */
int
ufs_mknod(v)
	void *v;
{
	struct vop_mknod_args /* {
				 struct vnode *a_dvp;
				 struct vnode **a_vpp;
				 struct componentname *a_cnp;
				 struct vattr *a_vap;
				 } */ *ap = v;
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
		ip->i_ffs_rdev = vap->va_rdev;
	}
	/*
	 * Remove inode so that it will be reloaded by VFS_VGET and
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.
	 */
	vput(*vpp);
	(*vpp)->v_type = VNON;
	vgone(*vpp);
	*vpp = 0;
	return (0);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
int
ufs_open(v)
	void *v;
{
	struct vop_open_args /* {
				struct vnode *a_vp;
				int  a_mode;
				struct ucred *a_cred;
				struct proc *a_p;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);

	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((ip->i_ffs_flags & APPEND) &&
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
/* ARGSUSED */
int
ufs_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	simple_lock(&vp->v_interlock);
	if (vp->v_usecount > 1) {
		struct timeval tv;

		getmicrotime(&tv);
		ITIMES(ip, &tv, &tv);
	}
	simple_unlock(&vp->v_interlock);
	return (0);
}

int
ufs_access(v)
	void *v;
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
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
	if ((mode & VWRITE) && (ip->i_ffs_flags & IMMUTABLE))
		return (EPERM);

	return (vaccess(ip->i_ffs_mode, ip->i_ffs_uid, ip->i_ffs_gid,
					mode, ap->a_cred));
}

/* ARGSUSED */
int
ufs_getattr(v)
	void *v;
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
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
	vap->va_mode = ip->i_ffs_mode & ~IFMT;
	vap->va_nlink = ip->i_effnlink;
	vap->va_uid = ip->i_ffs_uid;
	vap->va_gid = ip->i_ffs_gid;
	vap->va_rdev = (dev_t)ip->i_ffs_rdev;
	vap->va_size = ip->i_ffs_size;
	vap->va_atime.tv_sec = ip->i_ffs_atime;
	vap->va_atime.tv_nsec = ip->i_ffs_atimensec;
	vap->va_mtime.tv_sec = ip->i_ffs_mtime;
	vap->va_mtime.tv_nsec = ip->i_ffs_mtimensec;
	vap->va_ctime.tv_sec = ip->i_ffs_ctime;
	vap->va_ctime.tv_nsec = ip->i_ffs_ctimensec;
	vap->va_flags = ip->i_ffs_flags;
	vap->va_gen = ip->i_ffs_gen;
	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = dbtob((u_quad_t)ip->i_ffs_blocks);
	vap->va_type = vp->v_type;
	vap->va_filerev = ip->i_modrev;
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
int
ufs_setattr(v)
	void *v;
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
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
		if (cred->cr_uid != ip->i_ffs_uid &&
		    (error = suser_ucred(cred)))
			return (error);
		if (cred->cr_uid == 0) {
			if ((ip->i_ffs_flags & (SF_IMMUTABLE | SF_APPEND)) &&
			    securelevel > 0)
				return (EPERM);
			ip->i_ffs_flags = vap->va_flags;
		} else {
			if (ip->i_ffs_flags & (SF_IMMUTABLE | SF_APPEND) ||
			    (vap->va_flags & UF_SETTABLE) != vap->va_flags)
				return (EPERM);
			ip->i_ffs_flags &= SF_SETTABLE;
			ip->i_ffs_flags |= (vap->va_flags & UF_SETTABLE);
		}
		ip->i_flag |= IN_CHANGE;
		if (vap->va_flags & (IMMUTABLE | APPEND))
			return (0);
	}
	if (ip->i_ffs_flags & (IMMUTABLE | APPEND))
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
		oldsize = ip->i_ffs_size;
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
		if (cred->cr_uid != ip->i_ffs_uid &&
		    (error = suser_ucred(cred)) &&
		    ((vap->va_vaflags & VA_UTIMES_NULL) == 0 || 
		    (error = VOP_ACCESS(vp, VWRITE, cred, p))))
			return (error);
		if (vap->va_atime.tv_sec != VNOVAL)
			ip->i_flag |= IN_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL)
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
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
static int
ufs_chmod(vp, mode, cred, p)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{
	struct inode *ip = VTOI(vp);
	int error;

	if (cred->cr_uid != ip->i_ffs_uid &&
	    (error = suser_ucred(cred)))
		return (error);
	if (cred->cr_uid) {
		if (vp->v_type != VDIR && (mode & S_ISTXT))
			return (EFTYPE);
		if (!groupmember(ip->i_ffs_gid, cred) && (mode & ISGID))
			return (EPERM);
	}
	ip->i_ffs_mode &= ~ALLPERMS;
	ip->i_ffs_mode |= (mode & ALLPERMS);
	ip->i_flag |= IN_CHANGE;
	if ((vp->v_flag & VTEXT) && (ip->i_ffs_mode & S_ISTXT) == 0)
		(void) uvm_vnp_uncache(vp);
	return (0);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ufs_chown(vp, uid, gid, cred, p)
	struct vnode *vp;
	uid_t uid;
	gid_t gid;
	struct ucred *cred;
	struct proc *p;
{
	struct inode *ip = VTOI(vp);
	uid_t ouid;
	gid_t ogid;
	int error = 0;
	daddr_t change;
	enum ufs_quota_flags quota_flags = 0;

	if (uid == (uid_t)VNOVAL)
		uid = ip->i_ffs_uid;
	if (gid == (gid_t)VNOVAL)
		gid = ip->i_ffs_gid;
	/*
	 * If we don't own the file, are trying to change the owner
	 * of the file, or are not a member of the target group,
	 * the caller must be superuser or the call fails.
	 */
	if ((cred->cr_uid != ip->i_ffs_uid || uid != ip->i_ffs_uid ||
	    (gid != ip->i_ffs_gid && !groupmember((gid_t)gid, cred))) &&
	    (error = suser_ucred(cred)))
		return (error);
	ogid = ip->i_ffs_gid;
	ouid = ip->i_ffs_uid;
	change = ip->i_ffs_blocks;

	if (ouid == uid)
		quota_flags |= UFS_QUOTA_NOUID;
	
	if (ogid == gid)
		quota_flags |= UFS_QUOTA_NOGID;

	if ((error = getinoquota(ip)) != 0)
		return (error);
	(void) ufs_quota_free_blocks2(ip, change, cred, quota_flags);
	(void) ufs_quota_free_inode2(ip, cred, quota_flags);
	(void) ufs_quota_delete(ip);

	ip->i_ffs_gid = gid;
	ip->i_ffs_uid = uid;

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
		ip->i_ffs_mode &= ~ISUID;
	if (ogid != gid && cred->cr_uid != 0)
		ip->i_ffs_mode &= ~ISGID;
	return (0);

error:
	(void) ufs_quota_delete(ip);

	ip->i_ffs_gid = ogid;
	ip->i_ffs_uid = ouid;
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
ufs_ioctl(v)
	void *v;
{
#if 0
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
#endif
	return (ENOTTY);
}

/* ARGSUSED */
int
ufs_poll(v)
	void *v;
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct proc *a_p;
	} */ *ap = v;

	/*
	 * We should really check to see if I/O is possible.
	 */
	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Seek on a file
 *
 * Nothing to do, so just return.
 */
/* ARGSUSED */
int
ufs_seek(v)
	void *v;
{
#if 0
	struct vop_seek_args /* {
		struct vnode *a_vp;
		off_t  a_oldoff;
		off_t  a_newoff;
		struct ucred *a_cred;
	} */ *ap = v;
#endif

	return (0);
}

int
ufs_remove(v)
	void *v;
{
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct inode *ip;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VDIR || (ip->i_ffs_flags & (IMMUTABLE | APPEND)) ||
	    (VTOI(dvp)->i_ffs_flags & APPEND)) {
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
ufs_link(v)
	void *v;
{
	struct vop_link_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
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
	if ((nlink_t)ip->i_ffs_nlink >= LINK_MAX) {
		VOP_ABORTOP(dvp, cnp);
		error = EMLINK;
		goto out1;
	}
	if (ip->i_ffs_flags & (IMMUTABLE | APPEND)) {
		VOP_ABORTOP(dvp, cnp);
		error = EPERM;
		goto out1;
	}
	ip->i_effnlink++;
	ip->i_ffs_nlink++;
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(vp))
		softdep_change_linkcnt(ip);
	if ((error = UFS_UPDATE(ip, !DOINGSOFTDEP(vp))) == 0) {
		ufs_makedirentry(ip, cnp, &newdir);
		error = ufs_direnter(dvp, vp, &newdir, cnp, NULL);
	}
	if (error) {
		ip->i_effnlink--;
		ip->i_ffs_nlink--;
		ip->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(vp))
			softdep_change_linkcnt(ip);
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
 * whiteout vnode call
 */
int
ufs_whiteout(v)
	void *v;
{
	struct vop_whiteout_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
		int a_flags;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct direct newdir;
	int error = 0;

	switch (ap->a_flags) {
	case LOOKUP:
		/* 4.4 format directories support whiteout operations */
		if (dvp->v_mount->mnt_maxsymlinklen > 0)
			return (0);
		return (EOPNOTSUPP);

	case CREATE:
		/* create a new directory whiteout */
#ifdef DIAGNOSTIC
		if ((cnp->cn_flags & SAVENAME) == 0)
			panic("ufs_whiteout: missing name");
		if (dvp->v_mount->mnt_maxsymlinklen <= 0)
			panic("ufs_whiteout: old format filesystem");
#endif

		newdir.d_ino = WINO;
		newdir.d_namlen = cnp->cn_namelen;
		bcopy(cnp->cn_nameptr, newdir.d_name, (unsigned)cnp->cn_namelen + 1);
		newdir.d_type = DT_WHT;
		error = ufs_direnter(dvp, NULL, &newdir, cnp, NULL);
		break;

	case DELETE:
		/* remove an existing directory whiteout */
#ifdef DIAGNOSTIC
		if (dvp->v_mount->mnt_maxsymlinklen <= 0)
			panic("ufs_whiteout: old format filesystem");
#endif

		cnp->cn_flags &= ~DOWHITEOUT;
		error = ufs_dirremove(dvp, NULL, cnp->cn_flags, 0);
		break;
	default:
		panic("ufs_whiteout: unknown op");
		/* NOTREACHED */
	}
	if (cnp->cn_flags & HASBUF) {
		pool_put(&namei_pool, cnp->cn_pnbuf);
		cnp->cn_flags &= ~HASBUF;
	}
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
ufs_rename(v)
	void *v;
{
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
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
		VOP_ABORTOP(tdvp, tcnp); /* XXX, why not in NFS? */
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fdvp, fcnp); /* XXX, why not in NFS? */
		vrele(fdvp);
		vrele(fvp);
		return (error);
	}

	if (tvp && ((VTOI(tvp)->i_ffs_flags & (IMMUTABLE | APPEND)) ||
	    (VTOI(tdvp)->i_ffs_flags & APPEND))) {
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
		 * Relookup() may find a file that is unrelated to the
		 * original one, or it may fail.  Too bad.
		 */
		vrele(fvp);
		fcnp->cn_flags &= ~MODMASK;
		fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
		if ((fcnp->cn_flags & SAVESTART) == 0)
			panic("ufs_rename: lost from startdir");
		fcnp->cn_nameiop = DELETE;
		(void) relookup(fdvp, &fvp, fcnp);
		vrele(fdvp);
		if (fvp == NULL) {
			return (ENOENT);
		}
		return (VOP_REMOVE(fdvp, fvp, fcnp));
	}

	if ((error = vn_lock(fvp, LK_EXCLUSIVE, p)) != 0)
		goto abortit;

	/* fvp, tdvp, tvp now locked */
	dp = VTOI(fdvp);
	ip = VTOI(fvp);
	if ((nlink_t)ip->i_ffs_nlink >= LINK_MAX) {
		VOP_UNLOCK(fvp, 0, p);
		error = EMLINK;
		goto abortit;
	}
	if ((ip->i_ffs_flags & (IMMUTABLE | APPEND)) ||
	    (dp->i_ffs_flags & APPEND)) {
		VOP_UNLOCK(fvp, 0, p);
		error = EPERM;
		goto abortit;
	}
	if ((ip->i_ffs_mode & IFMT) == IFDIR) {
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
	ip->i_ffs_nlink++;
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(fvp))
		softdep_change_linkcnt(ip);
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

		/* Only tdvp is locked */
		if ((error = ufs_checkpath(ip, dp, tcnp->cn_cred)) != 0)
			goto out;
		if ((tcnp->cn_flags & SAVESTART) == 0)
			panic("ufs_rename: lost to startdir");
		if ((error = relookup(tdvp, &tvp, tcnp)) != 0)
			goto out;
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
			if ((nlink_t)dp->i_ffs_nlink >= LINK_MAX) {
				error = EMLINK;
				goto bad;
			}
			dp->i_effnlink++;
			dp->i_ffs_nlink++;
			dp->i_flag |= IN_CHANGE;
			if (DOINGSOFTDEP(tdvp))
                               softdep_change_linkcnt(dp);
			if ((error = UFS_UPDATE(dp, !DOINGSOFTDEP(tdvp))) 
			    != 0) {
				dp->i_effnlink--;
				dp->i_ffs_nlink--;
				dp->i_flag |= IN_CHANGE;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(dp);
				goto bad;
			}
		}
		ufs_makedirentry(ip, tcnp, &newdir);
		if ((error = ufs_direnter(tdvp, NULL, &newdir, tcnp, NULL)) != 0) {
			if (doingdirectory && newparent) {
				dp->i_effnlink--;
				dp->i_ffs_nlink--;
				dp->i_flag |= IN_CHANGE;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(dp);
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
		if ((dp->i_ffs_mode & S_ISTXT) && tcnp->cn_cred->cr_uid != 0 &&
		    tcnp->cn_cred->cr_uid != dp->i_ffs_uid &&
		    xp->i_ffs_uid != tcnp->cn_cred->cr_uid) {
			error = EPERM;
			goto bad;
		}
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if ((xp->i_ffs_mode & IFMT) == IFDIR) {
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
                   IFTODT(ip->i_ffs_mode), (doingdirectory && newparent) ?
		   newparent : doingdirectory)) != 0)
                        goto bad;
		if (doingdirectory) {
			if (!newparent) {
				dp->i_effnlink--;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(dp);
			}
			xp->i_effnlink--;
			if (DOINGSOFTDEP(tvp))
				softdep_change_linkcnt(xp);
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
				dp->i_ffs_nlink--;
				dp->i_flag |= IN_CHANGE;
			}

			xp->i_ffs_nlink--;
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
	(void) relookup(fdvp, &fvp, fcnp);
	vrele(fdvp);
	if (fvp != NULL) {
		xp = VTOI(fvp);
		dp = VTOI(fdvp);
	} else {
		/*
		 * From name has disappeared.
		 */
		if (doingdirectory)
			panic("ufs_rename: lost dir entry");
		vrele(ap->a_fvp);
		return (0);
	}
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
		ip->i_ffs_nlink--;
		ip->i_flag |= IN_CHANGE;
		ip->i_flag &= ~IN_RENAME;
		if (DOINGSOFTDEP(fvp))
			softdep_change_linkcnt(ip);
		vput(fvp);
	} else
		vrele(fvp);
	return (error);
}

/*
 * Mkdir system call
 */
int
ufs_mkdir(v)
	void *v;
{
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
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
	if ((nlink_t)dp->i_ffs_nlink >= LINK_MAX) {
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
	ip->i_ffs_uid = cnp->cn_cred->cr_uid;
	ip->i_ffs_gid = dp->i_ffs_gid;

	if ((error = getinoquota(ip)) ||
	    (error = ufs_quota_alloc_inode(ip, cnp->cn_cred))) {
		pool_put(&namei_pool, cnp->cn_pnbuf);
		UFS_INODE_FREE(ip, ip->i_number, dmode);
		vput(tvp);
		vput(dvp);
		return (error);
	}

	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_ffs_mode = dmode;
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 2;
	ip->i_ffs_nlink = 2;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);

	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_ffs_flags |= UF_OPAQUE;

	/*
	 * Bump link count in parent directory to reflect work done below.
	 * Should be done before reference is create so cleanup is 
	 * possible if we crash.
	 */
	dp->i_effnlink++;
	dp->i_ffs_nlink++;
	dp->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(dvp))
		softdep_change_linkcnt(dp);
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
	ip->i_ffs_size = DIRBLKSIZ;
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	uvm_vnp_setsize(tvp, ip->i_ffs_size);
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
         * Directory set up, now install it's entry in the parent directory.
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
		VN_KNOTE(dvp, NOTE_WRITE);
                *ap->a_vpp = tvp;
        } else {
                dp->i_effnlink--;
                dp->i_ffs_nlink--;
                dp->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(dvp))
			softdep_change_linkcnt(dp);
                /*
                 * No need to do an explicit VOP_TRUNCATE here, vrele will
                 * do this for us because we set the link count to 0.
                 */
                ip->i_effnlink = 0;
                ip->i_ffs_nlink = 0;
                ip->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(tvp))
			softdep_change_linkcnt(ip);
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
ufs_rmdir(v)
	void *v;
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
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
	if (dp == ip || vp->v_mountedhere != 0) {
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
	if ((dp->i_ffs_flags & APPEND) ||
		(ip->i_ffs_flags & (IMMUTABLE | APPEND))) {
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
		softdep_change_linkcnt(dp);
		softdep_change_linkcnt(ip);
	}
	if ((error = ufs_dirremove(dvp, ip, cnp->cn_flags, 1)) != 0) {
		dp->i_effnlink++;
		ip->i_effnlink++;
		if (DOINGSOFTDEP(vp)) {
			softdep_change_linkcnt(dp);
			softdep_change_linkcnt(ip);
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

		dp->i_ffs_nlink--;
		dp->i_flag |= IN_CHANGE;
		ip->i_ffs_nlink--;
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
ufs_symlink(v)
	void *v;
{
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
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
		bcopy(ap->a_target, (char *)ip->i_ffs_shortlink, len);
		ip->i_ffs_size = len;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	} else
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED, ap->a_cnp->cn_cred, NULL,
		    (struct proc *)0);
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
ufs_readdir(v)
	void *v;
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long **a_cookies;
		int *ncookies;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	int error;
	size_t count, lost;
	off_t off = uio->uio_offset;

	count = uio->uio_resid;
	/* Make sure we don't return partial entries. */
	count -= (uio->uio_offset + count) & (DIRBLKSIZ -1);
	if (count <= 0)
		return (EINVAL);
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
			MALLOC(dirbuf, caddr_t, count, M_TEMP, M_WAITOK);
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
			FREE(dirbuf, M_TEMP);
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
			(uio->uio_iov->iov_base - (uio->uio_offset - off));
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
                MALLOC(cookies, u_long *, ncookies * sizeof(u_long), M_TEMP,
                    M_WAITOK);
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
	*ap->a_eofflag = VTOI(ap->a_vp)->i_ffs_size <= uio->uio_offset;
	return (error);
}

/*
 * Return target name of a symbolic link
 */
int
ufs_readlink(v)
	void *v;
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	int isize;

	isize = ip->i_ffs_size;
	if (isize < vp->v_mount->mnt_maxsymlinklen ||
	    (vp->v_mount->mnt_maxsymlinklen == 0 && ip->i_ffs_blocks == 0)) {
		uiomove((char *)ip->i_ffs_shortlink, isize, ap->a_uio);
		return (0);
	}
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Lock an inode. If its already locked, set the WANT bit and sleep.
 */
int
ufs_lock(v)
	void *v;
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTOI(vp)->i_lock, ap->a_flags, &vp->v_interlock,
		ap->a_p));
}

/*
 * Unlock an inode.  If WANT bit is on, wakeup.
 */
int
ufs_unlock(v)
	void *v;
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTOI(vp)->i_lock, ap->a_flags | LK_RELEASE,
		&vp->v_interlock, ap->a_p));
}

/*
 * Check for a locked inode.
 */
int
ufs_islocked(v)
	void *v;
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	return (lockstatus(&VTOI(ap->a_vp)->i_lock));
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ufs_strategy(v)
	void *v;
{
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap = v;
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
		if ((long)bp->b_blkno == -1)
			clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		s = splbio();
		biodone(bp);
		splx(s);
		return (0);
	}
	vp = ip->i_devvp;
	bp->b_dev = vp->v_rdev;
	VOCALL (vp->v_op, VOFFSET(vop_strategy), ap);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
int
ufs_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	printf("tag VT_UFS, ino %d, on dev %d, %d", ip->i_number,
		major(ip->i_dev), minor(ip->i_dev));
	printf(" flags 0x%x, effnlink %d, nlink %d\n",
	       ip->i_flag, ip->i_effnlink, ip->i_ffs_nlink);
	printf("\tmode 0%o, owner %d, group %d, size %lld",
	       ip->i_ffs_mode, ip->i_ffs_uid, ip->i_ffs_gid, ip->i_ffs_size);

#ifdef FIFO
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
#endif /* FIFO */
	lockmgr_printinfo(&ip->i_lock);
	printf("\n");
	return (0);
}

/*
 * Read wrapper for special devices.
 */
int
ufsspec_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;

	/*
	 * Set access flag.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for special devices.
 */
int
ufsspec_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;

	/*
	 * Set update and change flags.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the inode then do device close.
 */
int
ufsspec_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	simple_lock(&vp->v_interlock);
	if (ap->a_vp->v_usecount > 1) {
		struct timeval tv;

		getmicrotime(&tv);
		ITIMES(ip, &tv, &tv);
	}
	simple_unlock(&vp->v_interlock);
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_close), ap));
}

#ifdef FIFO
/*
 * Read wrapper for fifo's
 */
int
ufsfifo_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	extern int (**fifo_vnodeop_p)(void *);

	/*
	 * Set access flag.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for fifo's.
 */
int
ufsfifo_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	extern int (**fifo_vnodeop_p)(void *);

	/*
	 * Set update and change flags.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for fifo's.
 *
 * Update the times on the inode then do device close.
 */
int
ufsfifo_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	extern int (**fifo_vnodeop_p)(void *);
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	simple_lock(&vp->v_interlock);
	if (ap->a_vp->v_usecount > 1) {
		struct timeval tv;

		getmicrotime(&tv);
		ITIMES(ip, &tv, &tv);
	}
	simple_unlock(&vp->v_interlock);
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_close), ap));
}
#endif /* FIFO */

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
int
ufs_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

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
ufs_advlock(v)
	void *v;
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);

	return (lf_advlock(&ip->i_lockf, ip->i_ffs_size, ap->a_id, ap->a_op,
	    ap->a_fl, ap->a_flags));
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
int
ufs_vinit(mntp, specops, fifoops, vpp)
	struct mount *mntp;
	int (**specops)(void *);
	int (**fifoops)(void *);
	struct vnode **vpp;
{
	struct inode *ip;
	struct vnode *vp, *nvp;
	struct timeval mtv;

	vp = *vpp;
	ip = VTOI(vp);
	switch(vp->v_type = IFTOVT(ip->i_ffs_mode)) {
	case VCHR:
	case VBLK:
		vp->v_op = specops;
		if ((nvp = checkalias(vp, ip->i_ffs_rdev, mntp)) != NULL) {
			/*
			 * Discard unneeded vnode, but save its inode.
			 * Note that the lock is carried over in the inode
			 * to the replacement vnode.
			 */
			nvp->v_data = vp->v_data;
			vp->v_data = NULL;
			vp->v_op = spec_vnodeop_p;
#ifdef DIAGNOSTIC
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
ufs_makeinode(mode, dvp, vpp, cnp)
	int mode;
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
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
	ip->i_ffs_gid = pdir->i_ffs_gid;
	ip->i_ffs_uid = cnp->cn_cred->cr_uid;

	if ((error = getinoquota(ip)) ||
	    (error = ufs_quota_alloc_inode(ip, cnp->cn_cred))) {
		pool_put(&namei_pool, cnp->cn_pnbuf);
		UFS_INODE_FREE(ip, ip->i_number, mode);
		vput(tvp);
		vput(dvp);
		return (error);
	}

	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_ffs_mode = mode;
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 1;
	ip->i_ffs_nlink = 1;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
	if ((ip->i_ffs_mode & ISGID) &&
		!groupmember(ip->i_ffs_gid, cnp->cn_cred) &&
	    suser_ucred(cnp->cn_cred))
		ip->i_ffs_mode &= ~ISGID;

	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_ffs_flags |= UF_OPAQUE;

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
	ip->i_ffs_nlink = 0;
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
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
ufs_kqfilter(v)
	void *v;
{
	struct vop_kqfilter_args /* {
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap = v;
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
		return (1);
	}

	kn->kn_hook = (caddr_t)vp;

	simple_lock(&vp->v_selectinfo.vsi_lock);
	SLIST_INSERT_HEAD(&vp->v_selectinfo.vsi_selinfo.si_note, kn, kn_selnext);
	simple_unlock(&vp->v_selectinfo.vsi_lock);

	return (0);
}

void
filt_ufsdetach(struct knote *kn)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;

	simple_lock(&vp->v_selectinfo.vsi_lock);
	SLIST_REMOVE(&vp->v_selectinfo.vsi_selinfo.si_note,
	    kn, knote, kn_selnext);
	simple_unlock(&vp->v_selectinfo.vsi_lock);
}

/*ARGSUSED*/
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

        kn->kn_data = ip->i_ffs_size - kn->kn_fp->f_offset;
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
