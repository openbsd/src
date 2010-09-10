/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * NNPFS operations.
 */

#ifdef __APPLE__
#define MACH_KERNEL 1
#endif

#include <nnpfs/nnpfs_locl.h>
#include <nnpfs/nnpfs_message.h>
#include <nnpfs/nnpfs_common.h>
#include <nnpfs/nnpfs_fs.h>
#include <nnpfs/nnpfs_dev.h>
#include <nnpfs/nnpfs_deb.h>
#include <nnpfs/nnpfs_syscalls.h>
#include <nnpfs/nnpfs_vnodeops.h>
#ifdef HAVE_VM_VNODE_PAGER_H
#include <vm/vnode_pager.h>
#endif

#include <sys/pool.h>

RCSID("$arla: nnpfs_vnodeops-bsd.c,v 1.123 2003/02/15 16:40:36 lha Exp $");

/*
 * vnode functions
 */

#ifdef HAVE_VOP_OPEN
int
nnpfs_open(struct vop_open_args * ap)
     /*
  struct vop_open {
          struct vnode *vp;
          int mode;
          struct ucred *cred;
          struct proc *p;
  }; */
{
#ifdef HAVE_FREEBSD_THREAD
    return nnpfs_open_common (ap->a_vp, ap->a_mode, ap->a_cred, ap->a_td);
#else
    return nnpfs_open_common (ap->a_vp, ap->a_mode, ap->a_cred, ap->a_p);
#endif
}
#endif /* HAVE_VOP_OPEN */

#ifdef HAVE_VOP_FSYNC
int
nnpfs_fsync(struct vop_fsync_args * ap)
     /*
  vop_fsync {
	struct vnode *vp;
	struct ucred *cred;
	int waitfor;
	struct proc *p;
};  */
{
#ifdef HAVE_STRUCT_VOP_FSYNC_ARGS_A_FLAGS
    return nnpfs_fsync_common(ap->a_vp, ap->a_cred, ap->a_flags, ap->a_p);
#else
#ifdef HAVE_FREEBSD_THREAD
    return nnpfs_fsync_common(ap->a_vp, ap->a_cred, ap->a_waitfor, ap->a_td);
#else
    return nnpfs_fsync_common(ap->a_vp, ap->a_cred, ap->a_waitfor, ap->a_p);
#endif
#endif
}
#endif /* HAVE_VOP_FSYNC */

#ifdef HAVE_VOP_CLOSE 
int
nnpfs_close(struct vop_close_args * ap)
     /* vop_close {
	IN struct vnode *vp;
	IN int fflag;
	IN struct ucred *cred;
	IN struct proc *p;
  }; */
{
#ifdef HAVE_FREEBSD_THREAD
    return nnpfs_close_common(ap->a_vp, ap->a_fflag, ap->a_td, ap->a_cred);
#else
    return nnpfs_close_common(ap->a_vp, ap->a_fflag, ap->a_p, ap->a_cred);
#endif
}
#endif /* HAVE_VOP_CLOSE */

#ifdef HAVE_VOP_READ
int
nnpfs_read(struct vop_read_args * ap)
     /* vop_read {
	IN struct vnode *vp;
	INOUT struct uio *uio;
	IN int ioflag;
	IN struct ucred *cred;
   }; */
{
    return nnpfs_read_common(ap->a_vp, ap->a_uio, ap->a_ioflag, ap->a_cred);
}
#endif /* HAVE_VOP_READ */

#ifdef HAVE_VOP_WRITE
int
nnpfs_write(struct vop_write_args * ap)
     /* vop_write {
	IN struct vnode *vp;
	INOUT struct uio *uio;
	IN int ioflag;
	IN struct ucred *cred;
   }; */
{
    return nnpfs_write_common(ap->a_vp, ap->a_uio, ap->a_ioflag, ap->a_cred);
}
#endif /* HAVE_VOP_WRITE */

#ifdef HAVE_VOP_IOCTL
int
nnpfs_ioctl(struct vop_ioctl_args * ap)
     /* struct vnode *vp,
	  int com,
	  caddr_t data,
	  int flag,
	  struct ucred *cred) */
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_ioctl\n"));

    return EOPNOTSUPP;
}
#endif /* HAVE_VOP_IOCTL */

#ifdef HAVE_VOP_SELECT
int
nnpfs_select(struct vop_select_args * ap)
     /* struct vnode *vp,
	   int which,
	   struct ucred *cred ) */
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_select\n"));

    return EOPNOTSUPP;
}
#endif /* HAVE_VOP_SELECT */

#ifdef HAVE_VOP_SEEK
int
nnpfs_seek(struct vop_seek_args * ap)
     /*
struct vop_seek_args {
        struct vnodeop_desc *a_desc;
        struct vnode *a_vp;
        off_t a_oldoff;
        off_t a_newoff;
        struct ucred *a_cred;
};
*/
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_seek\n"));
    return 0;
}
#endif /* HAVE_VOP_SEEK */

#ifdef HAVE_VOP_POLL
int
nnpfs_poll(struct vop_poll_args * ap)
     /* vop_poll {
	IN struct vnode *vp;
	IN int events;
	IN struct proc *p;
   }; */
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_poll\n"));
    return EOPNOTSUPP;
}
#endif /* HAVE_VOP_POLL */

#ifdef HAVE_VOP_GETATTR
int
nnpfs_getattr(struct vop_getattr_args * ap)
     /* struct vnode *vp,
	    struct vattr *vap,
	    struct ucred *cred,
	    struct proc *p) */
{
#ifdef HAVE_FREEBSD_THREAD
    return nnpfs_getattr_common(ap->a_vp, ap->a_vap, ap->a_cred, ap->a_td);
#else
    return nnpfs_getattr_common(ap->a_vp, ap->a_vap, ap->a_cred, ap->a_p);
#endif
}
#endif /* HAVE_VOP_GETATTR */

#ifdef HAVE_VOP_SETATTR
int
nnpfs_setattr(struct vop_setattr_args * ap)
     /* struct vnode *vp,
	    struct vattr *vap,
	    struct ucred *cred,
	    struct proc *p)
	    */
{
#ifdef HAVE_FREEBSD_THREAD
    return nnpfs_setattr_common(ap->a_vp, ap->a_vap, ap->a_cred, ap->a_td);
#else
    return nnpfs_setattr_common(ap->a_vp, ap->a_vap, ap->a_cred, ap->a_p);
#endif
}
#endif /* HAVE_VOP_SETATTR */

#ifdef HAVE_VOP_ACCESS
int
nnpfs_access(struct vop_access_args * ap)
     /*
struct vnode *vp,
	   int mode,
	   struct ucred *cred,
	   struct proc *p)
	   */
{
#ifdef HAVE_FREEBSD_THREAD
    return nnpfs_access_common(ap->a_vp, ap->a_mode, ap->a_cred, ap->a_td);
#else
    return nnpfs_access_common(ap->a_vp, ap->a_mode, ap->a_cred, ap->a_p);
#endif
}
#endif /* HAVE_VOP_ACCESS */

#ifdef HAVE_VOP_LOOKUP
int
nnpfs_lookup(struct vop_lookup_args * ap)
     /* struct vop_lookup_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
}; */
{
    struct componentname *cnp = ap->a_cnp;
    int error;
    int lockparent = (cnp->cn_flags & (LOCKPARENT | ISLASTCN))
	== (LOCKPARENT | ISLASTCN);

    NNPFSDEB(XDEBVNOPS, ("nnpfs_lookup: (%s, %ld), nameiop = %lu, flags = %lu\n",
		       cnp->cn_nameptr,
		       cnp->cn_namelen,
		       cnp->cn_nameiop,
		       cnp->cn_flags));

#ifdef PDIRUNLOCK
    cnp->cn_flags &= ~PDIRUNLOCK;
#endif

    error = nnpfs_lookup_common(ap->a_dvp, cnp, ap->a_vpp);

    if (error == ENOENT
	&& (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME)
	&& (cnp->cn_flags & ISLASTCN)) {
	error = EJUSTRETURN;
    }

    if (cnp->cn_nameiop != LOOKUP && cnp->cn_flags & ISLASTCN)
	cnp->cn_flags |= SAVENAME;

    if (error == 0 || error == EJUSTRETURN) {
	if (ap->a_dvp == *(ap->a_vpp)) {
	    /* if we looked up ourself, do nothing */
	} else if (!(cnp->cn_flags & ISLASTCN) || !lockparent) {
	    /* if we isn't last component and is isn't requested,
	     * return parent unlocked */
#ifdef HAVE_FREEBSD_THREAD
	    nnpfs_vfs_unlock (ap->a_dvp, nnpfs_cnp_to_thread(cnp));
#else
	    nnpfs_vfs_unlock (ap->a_dvp, nnpfs_cnp_to_proc(cnp));
#endif
#ifdef PDIRUNLOCK
	    cnp->cn_flags |= PDIRUNLOCK;
#endif
	}
    } else {
	/* in case of a error do nothing  */
    } 
    
    NNPFSDEB(XDEBVNOPS, ("nnpfs_lookup: error = %d\n", error));

    return error;
}
#endif /* HAVE_VOP_LOOKUP */

#ifdef HAVE_VOP_CACHEDLOOKUP
int
nnpfs_cachedlookup(struct vop_cachedlookup_args * ap)
     /* struct vop_cachedlookup_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
}; */
{
    return nnpfs_lookup((struct vop_lookup_args *)ap);
}
#endif /* HAVE_VOP_CACHEDLOOKUP */

/*
 * whatever clean-ups are needed for a componentname.
 */

static void
cleanup_cnp (struct componentname *cnp, int error)
{
    if (error != 0 || (cnp->cn_flags & SAVESTART) == 0) {
#if defined(HAVE_KERNEL_ZFREEI)
	zfreei(namei_zone, cnp->cn_pnbuf);
	cnp->cn_flags &= ~HASBUF;
#elif defined(HAVE_KERNEL_UMA_ZFREE_ARG)
	uma_zfree_arg(namei_zone, cnp->cn_pnbuf, NULL);
	cnp->cn_flags &= ~HASBUF;
#elif defined(FREE_ZONE)
	FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#elif defined(HAVE_KERNEL_ZFREE)
	zfree(namei_zone, cnp->cn_pnbuf);
	cnp->cn_flags &= ~HASBUF;
#elif defined(PNBUF_PUT)
	PNBUF_PUT(cnp->cn_pnbuf);
#else
	pool_put(&namei_pool, cnp->cn_pnbuf);
#endif
    }
}

#ifdef HAVE_VOP_CREATE
int
nnpfs_create(struct vop_create_args *ap)
{
    struct vnode *dvp  = ap->a_dvp;
    struct componentname *cnp = ap->a_cnp;
    const char *name   = cnp->cn_nameptr;
    struct ucred *cred = cnp->cn_cred;
#ifdef HAVE_FREEBSD_THREAD
    d_thread_t *p     = nnpfs_cnp_to_thread(cnp);
#else
    d_thread_t *p     = nnpfs_cnp_to_proc(cnp);
#endif
    int error;

    error = nnpfs_create_common(dvp, name, ap->a_vap, cred, p);

    if (error == 0) {
	error = nnpfs_lookup_common(dvp, cnp, ap->a_vpp);
    }

    cleanup_cnp (cnp, error);

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
    vput (dvp);
#endif

    NNPFSDEB(XDEBVNOPS, ("nnpfs_create: error = %d\n", error));
    
    return error;
}
#endif /* HAVE_VOP_CREATE */

#ifdef HAVE_VOP_REMOVE
int
nnpfs_remove(struct vop_remove_args * ap)
     /* struct vnode *dvp,
   struct vnode *vp,
   struct componentname *cnp */
{
    struct componentname *cnp = ap->a_cnp;
    struct vnode *dvp = ap->a_dvp;
    struct vnode *vp  = ap->a_vp;

#ifdef HAVE_FREEBSD_THREAD
    int error = nnpfs_remove_common(dvp, vp, cnp->cn_nameptr, 
				  cnp->cn_cred, nnpfs_cnp_to_thread(cnp));
#else
    int error = nnpfs_remove_common(dvp, vp, cnp->cn_nameptr, 
				  cnp->cn_cred, nnpfs_cnp_to_proc(cnp));
#endif

    cleanup_cnp (cnp, error);

#if !defined(__FreeBSD__) || __FreeBSD_version < 300000
    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);
    vput(dvp);
#endif
    
#ifdef __APPLE__
    if (error == 0) {
	if (UBCINFOEXISTS(vp)) {
	    ubc_setsize(vp, 0);
	    ubc_release(vp);
	    ubc_uncache(vp);
	}
    }
#endif

    return error;
}
#endif /* HAVE_VOP_REMOVE */

#ifdef HAVE_VOP_RENAME
int
nnpfs_rename(struct vop_rename_args * ap)
     /* vop_rename {
	IN WILLRELE struct vnode *fdvp;
	IN WILLRELE struct vnode *fvp;
	IN struct componentname *fcnp;
	IN WILLRELE struct vnode *tdvp;
	IN WILLRELE struct vnode *tvp;
	IN struct componentname *tcnp;
  }; */
{
    struct vnode *tdvp = ap->a_tdvp;
    struct vnode *tvp  = ap->a_tvp;
    struct vnode *fdvp = ap->a_fdvp;
    struct vnode *fvp  = ap->a_fvp;

    int error = nnpfs_rename_common(fdvp,
				  fvp,
				  ap->a_fcnp->cn_nameptr,
				  tdvp,
				  tvp,
				  ap->a_tcnp->cn_nameptr,
				  ap->a_tcnp->cn_cred,
#ifdef HAVE_FREEBSD_THREAD
				  nnpfs_cnp_to_thread (ap->a_fcnp));
#else
				  nnpfs_cnp_to_proc (ap->a_fcnp));
#endif
    if(tdvp == tvp)
	vrele(tdvp);
    else
	vput(tdvp);
    if(tvp)
	vput(tvp);
    vrele(fdvp);
    vrele(fvp);
    return error;
}
#endif /* HAVE_VOP_RENAME */

#ifdef HAVE_VOP_MKDIR
int
nnpfs_mkdir(struct vop_mkdir_args * ap)
     /* struct vnode *dvp,
	  char *nm,
	  struct vattr *va,
	  struct vnode **vpp,
	  struct ucred *cred)      */
{
    struct vnode *dvp  = ap->a_dvp;
    struct componentname *cnp = ap->a_cnp;
    const char *name   = cnp->cn_nameptr;
    struct ucred *cred = cnp->cn_cred;
#ifdef HAVE_FREEBSD_THREAD
    d_thread_t *p     = nnpfs_cnp_to_thread(cnp);
#else
    d_thread_t *p     = nnpfs_cnp_to_proc(cnp);
#endif
    int error;

    error = nnpfs_mkdir_common(dvp, name, ap->a_vap, cred, p);

    if (error == 0)
	error = nnpfs_lookup_common(dvp, cnp, ap->a_vpp);

    cleanup_cnp (cnp, error);

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    vput(dvp);
#endif

    NNPFSDEB(XDEBVNOPS, ("nnpfs_mkdir: error = %d\n", error));

    return error;
}
#endif /* HAVE_VOP_MKDIR */

#ifdef HAVE_VOP_RMDIR
int
nnpfs_rmdir(struct vop_rmdir_args * ap)
     /* struct vnode *dvp,
   struct vnode *vp,
   struct componentname *cnp */
{
    struct componentname *cnp = ap->a_cnp;
    struct vnode *dvp = ap->a_dvp;
    struct vnode *vp  = ap->a_vp;
    int error = nnpfs_rmdir_common(ap->a_dvp, ap->a_vp, 
				 cnp->cn_nameptr,
				 cnp->cn_cred,
#ifdef HAVE_FREEBSD_THREAD
				 nnpfs_cnp_to_thread(cnp));
#else
				 nnpfs_cnp_to_proc(cnp));
#endif

    cleanup_cnp (cnp, error);
#if !defined(__FreeBSD__) || __FreeBSD_version < 300000
    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);
    vput(dvp);
#endif

    return error;
}
#endif /* HAVE_VOP_RMDIR */

#ifdef HAVE_VOP_READDIR

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
typedef u_long nnpfs_cookie_t;
#elif defined(__NetBSD__)
typedef off_t nnpfs_cookie_t;
#else
#error dunno want kind of cookies you have
#endif

int
nnpfs_readdir(struct vop_readdir_args * ap)
     /* struct vnode *vp,
	    struct uio *uiop,
	    struct ucred *cred) */
{
    int error;
    off_t off;

    off = ap->a_uio->uio_offset;

    error = nnpfs_readdir_common(ap->a_vp, ap->a_uio, ap->a_cred,
#ifdef HAVE_FREEBSD_THREAD
			       nnpfs_uio_to_thread (ap->a_uio),
#else
			       nnpfs_uio_to_proc (ap->a_uio),
#endif
			       ap->a_eofflag);

    if (!error && ap->a_ncookies != NULL) {
	struct uio *uio = ap->a_uio;
	const struct dirent *dp, *dp_start, *dp_end;
	int ncookies;
	nnpfs_cookie_t *cookies, *cookiep;

	if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
	    panic("nnpfs_readdir: mail arla-drinkers and tell them to bake burned cookies");
	dp = (const struct dirent *)
	    ((const char *)uio->uio_iov->iov_base - (uio->uio_offset - off));

	dp_end = (const struct dirent *) uio->uio_iov->iov_base;
	for (dp_start = dp, ncookies = 0;
	     dp < dp_end;
	     dp = (const struct dirent *)((const char *) dp + dp->d_reclen)) {
	    if (dp->d_reclen <= 0)
		break;
	    ncookies++;
	}

	cookies = malloc(ncookies * sizeof(nnpfs_cookie_t), M_TEMP, M_WAITOK);
	for (dp = dp_start, cookiep = cookies;
	     dp < dp_end;
	     dp = (const struct dirent *)((const char *) dp + dp->d_reclen)) {
	    if (dp->d_reclen <= 0)
		break;
	    off += dp->d_reclen;
	    *cookiep++ = off;
	}
	*ap->a_cookies = cookies;
	*ap->a_ncookies = ncookies;
    }
    return error;
}
#endif /* HAVE_VOP_READDIR */

#ifdef HAVE_VOP_LINK
int
nnpfs_link(struct vop_link_args * ap)
     /*
	WILLRELE struct vnode *tdvp;
	struct vnode *vp;
	struct componentname *cnp;
	*/
{
    struct componentname *cnp = ap->a_cnp;
    struct vnode *vp = ap->a_vp;
    struct vnode *dvp;
#ifdef HAVE_FREEBSD_THREAD
    d_thread_t *p = cnp->cn_thread;
#else
    d_thread_t *p = cnp->cn_proc;
#endif
    int error;

#if defined (__OpenBSD__) || defined(__NetBSD__)
    dvp = ap->a_dvp;
#elif defined(__FreeBSD__) || defined(__APPLE__)
    dvp = ap->a_tdvp;
#else
#error what kind of BSD is this?
#endif

    if (vp->v_type == VDIR) {
#ifdef HAVE_VOP_ABORTOP
	    VOP_ABORTOP(dvp, cnp);
#endif
	    error = EPERM;
	    goto out;
    }
    if (dvp->v_mount != vp->v_mount) {
#ifdef HAVE_VOP_ABORTOP
	    VOP_ABORTOP(dvp, cnp);
#endif
	    error = EXDEV;
	    goto out;
    }
    /* FreeBSD 5.0 doesn't need to lock the vnode in VOP_LINK */
#if !defined(__FreeBSD_version) || __FreeBSD_version < 500043

    if (dvp != vp && (error = nnpfs_vfs_writelock(vp, p))) {
#ifdef HAVE_VOP_ABORTOP
	    VOP_ABORTOP(dvp, cnp);
#endif
	    goto out;
    }
#endif /* defined(__FreeBSD_version) || __FreeBSD_version < 500043 */

    error = nnpfs_link_common(
			   dvp,
			   vp,
			   cnp->cn_nameptr,
			   cnp->cn_cred,
#ifdef HAVE_FREEBSD_THREAD
			   nnpfs_cnp_to_thread (cnp));
#else
			   nnpfs_cnp_to_proc (cnp));
#endif

    cleanup_cnp (cnp, error);

    if (dvp != vp)
	nnpfs_vfs_unlock(vp, p);

out:
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    vput(dvp);
#endif

    return error;
}
#endif /* HAVE_VOP_LINK */

#ifdef HAVE_VOP_SYMLINK
int
nnpfs_symlink(struct vop_symlink_args * ap)
     /*
  IN WILLRELE struct vnode *dvp;
  OUT WILLRELE struct vnode **vpp;
  IN struct componentname *cnp;
  IN struct vattr *vap;
  IN char *target;
  */
{
    struct componentname *cnp = ap->a_cnp;
    struct vnode *dvp  = ap->a_dvp;
    struct vnode **vpp = ap->a_vpp;
    
    int error = nnpfs_symlink_common(dvp,
				   vpp,
				   cnp,
				   ap->a_vap,
				   ap->a_target);

    if (error == 0) {
	error = nnpfs_lookup_common(dvp, cnp, vpp);
	if (error == 0)
	    vput (*vpp);
    }
    cleanup_cnp (cnp, error);
#if !defined(__FreeBSD__)
    vput(dvp);
#endif
    return error;
}
#endif /* HAVE_VOP_SYMLINK */


#ifdef HAVE_VOP_READLINK
int
nnpfs_readlink(struct vop_readlink_args * ap)
     /* struct vnode *vp,
	     struct uio *uiop,
	     struct ucred *cred) */
{
    return nnpfs_readlink_common(ap->a_vp, ap->a_uio, ap->a_cred);
}
#endif /* HAVE_VOP_READLINK */

#ifdef HAVE_VOP_INACTIVE
int
nnpfs_inactive(struct vop_inactive_args * ap)
     /*struct vnode *vp,
	     struct ucred *cred)*/
{
#ifdef HAVE_FREEBSD_THREAD
    return nnpfs_inactive_common(ap->a_vp, nnpfs_curthread());
#else
    return nnpfs_inactive_common(ap->a_vp, nnpfs_curproc());
#endif
}
#endif /* HAVE_VOP_INACTICE */

#ifdef HAVE_VOP_RECLAIM
int
nnpfs_reclaim(struct vop_reclaim_args * ap)
     /*struct vop_reclaim_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};*/
{
    struct vnode *vp = ap->a_vp;
    int ret;

    ret = nnpfs_reclaim_common(vp);
    vp->v_data = NULL;
    return ret;
}
#endif /* HAVE_VOP_RECLAIM */

/*
 * Do lock, unlock, and islocked with lockmgr if we have it.
 */

#if defined(HAVE_KERNEL_LOCKMGR) || defined(HAVE_KERNEL_DEBUGLOCKMGR)

#ifdef HAVE_VOP_LOCK
int
nnpfs_lock(struct vop_lock_args * ap)
{               
    struct vnode *vp    = ap->a_vp;
    struct nnpfs_node *xn = VNODE_TO_XNODE(vp);
    nnpfs_vnode_lock *l   = &xn->lock;
    int flags           = ap->a_flags;
    int ret;

    NNPFSDEB(XDEBVNOPS, ("nnpfs_lock: %lx, flags 0x%x\n",
		       (unsigned long)vp, flags));

    if (l == NULL)
      panic("nnpfs_lock: lock NULL");

    NNPFSDEB(XDEBVNOPS, ("nnpfs_lock before: lk flags: %d share: %d "
			 "wait: %d excl: %d holder: 0x%llx\n",
			 l->lk_flags, l->lk_sharecount, l->lk_waitcount,
			 l->lk_exclusivecount,
			 (unsigned long long)
			 (nnpfs_uintptr_t)l->lk_lockholder));

#ifndef	DEBUG_LOCKS
#ifdef HAVE_FOUR_ARGUMENT_LOCKMGR
#ifdef HAVE_FREEBSD_THREAD
    ret = lockmgr(l, flags, &vp->v_interlock, ap->a_td);
#else
    ret = lockmgr(l, flags, &vp->v_interlock, ap->a_p);
#endif
#else
    ret = lockmgr(l, flags, NULL);
#endif
#else
#ifdef HAVE_FREEBSD_THREAD
    ret = debuglockmgr(l, flags, &vp->v_interlock, ap->a_td,
			"nnpfs_lock", ap->a_vp->filename, ap->a_vp->line);
#else
    ret = debuglockmgr(l, flags, &vp->v_interlock, ap->a_p,
			"nnpfs_lock", ap->a_vp->filename, ap->a_vp->line);
#endif
#endif
    NNPFSDEB(XDEBVNOPS, ("nnpfs_lock: lk flags: %d share: %d "
			 "wait: %d excl: %d holder: 0x%llx\n",
			 l->lk_flags, l->lk_sharecount, l->lk_waitcount,
			 l->lk_exclusivecount, 
			 (unsigned long long)
			 (nnpfs_uintptr_t)l->lk_lockholder));
    return ret;
}
#endif /* HAVE_VOP_LOCK */

#ifdef HAVE_VOP_UNLOCK
int
nnpfs_unlock(struct vop_unlock_args * ap)
{
    struct vnode *vp    = ap->a_vp;
    struct nnpfs_node *xn = VNODE_TO_XNODE(vp);
    nnpfs_vnode_lock *l   = &xn->lock;
    int flags           = ap->a_flags;
    int ret;

    if (l == NULL)
      panic("nnpfs_unlock: lock NULL");

    NNPFSDEB(XDEBVNOPS,
	   ("nnpfs_unlock: %lx, flags 0x%x, l %lx, ap %lx\n",
	    (unsigned long)vp, flags,
	    (unsigned long)l,
	    (unsigned long)ap));

    NNPFSDEB(XDEBVNOPS, ("nnpfs_unlock: lk flags: %d share: %d "
			 "wait: %d excl: %d holder: 0x%lld\n",
			 l->lk_flags, l->lk_sharecount, l->lk_waitcount,
			 l->lk_exclusivecount, 
			 (unsigned long long)
			 (nnpfs_uintptr_t)l->lk_lockholder));
#ifndef	DEBUG_LOCKS
#ifdef HAVE_FOUR_ARGUMENT_LOCKMGR
#ifdef HAVE_FREEBSD_THREAD
    ret = lockmgr (l, flags | LK_RELEASE, &vp->v_interlock, ap->a_td);
#else
    ret = lockmgr (l, flags | LK_RELEASE, &vp->v_interlock, ap->a_p);
#endif
#else
    ret = lockmgr (l, flags | LK_RELEASE, NULL);
#endif
#else
#ifdef HAVE_FREEBSD_THREAD
    ret = debuglockmgr (l, flags | LK_RELEASE, &vp->v_interlock, ap->a_td,
			"nnpfs_lock", ap->a_vp->filename, ap->a_vp->line);
#else
    ret = debuglockmgr (l, flags | LK_RELEASE, &vp->v_interlock, ap->a_p,
			"nnpfs_lock", ap->a_vp->filename, ap->a_vp->line);
#endif
#endif
    NNPFSDEB(XDEBVNOPS, ("nnpfs_unlock: return %d\n", ret));
    return ret;
}
#endif /* HAVE_VOP_UNLOCK */

#ifdef HAVE_VOP_ISLOCKED
int
nnpfs_islocked (struct vop_islocked_args *ap)
{
    struct vnode *vp    = ap->a_vp;
    struct nnpfs_node *xn = VNODE_TO_XNODE(vp);
    nnpfs_vnode_lock *l   = &xn->lock;

    NNPFSDEB(XDEBVNOPS, ("nnpfs_islocked: %lx\n",
		       (unsigned long)vp));

#if defined(HAVE_TWO_ARGUMENT_LOCKSTATUS)
#ifdef HAVE_FREEBSD_THREAD
    return lockstatus (l, ap->a_td);
#else
    return lockstatus (l, ap->a_p);
#endif
#elif defined(HAVE_ONE_ARGUMENT_LOCKSTATUS)
    return lockstatus (l);
#else
#error what lockstatus?
#endif
}
#endif /* HAVE_VOP_ISLOCKED */

#else /* !HAVE_KERNEL_LOCKMGR && !HAVE_KERNEL_DEBUGLOCKMGR */

#ifdef HAVE_VOP_LOCK
int
nnpfs_lock(struct vop_lock_args * ap)
{
    struct vnode *vp    = ap->a_vp;
    struct nnpfs_node *xn = VNODE_TO_XNODE(vp);

    NNPFSDEB(XDEBVNOPS, ("nnpfs_lock: %lx, %d\n",
		       (unsigned long)vp, xn->vnlocks));

    while (vp->v_flag & VXLOCK) {
	vp->v_flag |= VXWANT;
	(void) tsleep((caddr_t)vp, PINOD, "nnpfs_vnlock", 0);
    }
    if (vp->v_tag == VT_NON)
	return (ENOENT);
    ++xn->vnlocks;
    return 0;
}
#endif /* HAVE_VOP_LOCK */

#ifdef HAVE_VOP_UNLOCK
int
nnpfs_unlock(struct vop_unlock_args * ap)
{
    struct vnode *vp    = ap->a_vp;
    struct nnpfs_node *xn = VNODE_TO_XNODE(vp);
    NNPFSDEB(XDEBVNOPS, ("nnpfs_unlock: %lx, %d\n",
		       (unsigned long)vp, xn->vnlocks));

    --xn->vnlocks;
    if (xn->vnlocks < 0) {
	printf ("PANIC: nnpfs_unlock: unlocking unlocked\n");
	xn->vnlocks = 0;
    }
    NNPFSDEB(XDEBVNOPS, ("nnpfs_unlock: return\n"));

    return 0;
}
#endif /* HAVE_VOP_UNLOCK */

#ifdef HAVE_VOP_ISLOCKED
int
nnpfs_islocked (struct vop_islocked_args *ap)
{
    struct vnode *vp    = ap->a_vp;
    struct nnpfs_node *xn = VNODE_TO_XNODE(vp);

    NNPFSDEB(XDEBVNOPS, ("nnpfs_islocked: %lx, %d\n",
		       (unsigned long)vp, xn->vnlocks));

    return xn->vnlocks;
}
#endif /* HAVE_VOP_ISLOCKED */
#endif /* !HAVE_KERNEL_LOCKMGR */

#ifdef HAVE_VOP_ABORTOP
int
nnpfs_abortop (struct vop_abortop_args *ap)
     /* struct vnode *dvp;
   struct componentname *cnp; */
{
    struct componentname *cnp = ap->a_cnp;

    if ((cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
#if defined(HAVE_KERNEL_ZFREEI)
	zfreei(namei_zone, cnp->cn_pnbuf);
	ap->a_cnp->cn_flags &= ~HASBUF;
#elif defined(HAVE_KERNEL_UMA_ZFREE_ARG)
	uma_zfree_arg(namei_zone, cnp->cn_pnbuf, NULL);
	cnp->cn_flags &= ~HASBUF;
#elif defined(FREE_ZONE)
	FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#elif defined(HAVE_KERNEL_ZFREE)
	zfree(namei_zone, cnp->cn_pnbuf);
	ap->a_cnp->cn_flags &= ~HASBUF;
#elif defined(PNBUF_PUT)
	PNBUF_PUT(cnp->cn_pnbuf);
#else
	pool_put(&namei_pool, cnp->cn_pnbuf);
#endif
    return 0;
}
#endif /* HAVE_VOP_ABORTOP */

#ifdef HAVE_VOP_MMAP
int
nnpfs_mmap(struct vop_mmap_args *ap)
     /*
	IN struct vnode *vp;
	IN int fflags;
	IN struct ucred *cred;
	IN struct proc *p;
	*/
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_mmap\n"));
#ifdef HAVE_KERNEL_GENFS_MMAP
    return genfs_mmap(ap);
#else
    return EOPNOTSUPP;
#endif
}
#endif /* HAVE_VOP_MMAP */

#ifdef HAVE_VOP_BMAP
int
nnpfs_bmap(struct vop_bmap_args *ap)
     /*	IN struct vnode *vp;
	IN daddr64_t bn;
	OUT struct vnode **vpp;
	IN daddr64_t *bnp;
	OUT int *runp;
	OUT int *runb;
	*/
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_bmap\n"));
    return EOPNOTSUPP;
}
#endif /* HAVE_VOP_BMAP */

#ifdef HAVE_VOP_GETPAGES

static size_t
get_pages_endlength (struct vop_getpages_args *ap)
{
#ifdef HAVE_STRUCT_VOP_GETPAGES_ARGS_A_OFFSET
    /* NetBSD ubc */
    return (ap->a_offset << PAGE_SHIFT) + *ap->a_count * PAGE_SIZE;
#else
    return (ap->a_reqpage << PAGE_SHIFT) +  ap->a_count * PAGE_SIZE;
#endif
}

int
nnpfs_getpages (struct vop_getpages_args *ap)
     /* Old BSD
	IN struct vnode *vp;
	IN vm_page_t *m;
	IN int count;
	IN int reqpage;
	IN vm_ooffset_t offset;
     */
    /* NetBSD UBC
	IN struct vnode *vp;
	IN voff_t offset;
	IN vm_page_t *m;
	IN int *count;
	IN int centeridx;
	IN vm_prot_t access_type;
	IN int advice;
	IN int flags;
    */
{
    int error;

    NNPFSDEB(XDEBVNOPS, ("nnpfs_getpages\n"));

#if HAVE_KERNEL_VNODE_PAGER_GENERIC_GETPAGES
    error = vnode_pager_generic_getpages (ap->a_vp, ap->a_m, 
					  ap->a_count, ap->a_reqpage);
#else
    error = nnpfs_data_valid (ap->a_vp, VNODE_TO_XNODE(ap->a_vp)->rd_cred,
			    nnpfs_curproc(), NNPFS_DATA_R,
			    get_pages_endlength(ap));
    if (error == 0)
	error = VOP_GETPAGES(DATA_FROM_VNODE(ap->a_vp), 
			     ap->a_offset, ap->a_m,
			     ap->a_count, ap->a_centeridx, ap->a_access_type,
			     ap->a_advice, ap->a_flags);
#endif
    NNPFSDEB(XDEBVNOPS, ("nnpfs_getpages = %d\n", error));
    return error;
}
#endif /* HAVE_VOP_GETPAGES */

#ifdef HAVE_VOP_PUTPAGES
int
nnpfs_putpages (struct vop_putpages_args *ap)
     /* Old BSD
        IN struct vnode *vp;
        IN vm_page_t *m;
        IN int count;
        IN int sync;
        IN int *rtvals;
        IN vm_ooffset_t offset;
     */
    /* NetBSD UBC (>= 1.5Y)
	IN struct vnode *vp;
	IN voff_t offlo;
	IN voff_t offhi;
	IN int flags;
    */
{
    struct vnode *vp    = ap->a_vp;
    struct nnpfs_node *xn = VNODE_TO_XNODE(vp);
    struct vnode *t     = DATA_FROM_XNODE(xn);
    int error;

    NNPFSDEB(XDEBVNOPS, ("nnpfs_putpages\n"));

    if (t == NULL)
	return 0;

#ifdef HAVE_STRUCT_VOP_PUTPAGES_ARGS_A_SYNC /* FreeBSD-style */
    xn->flags |= NNPFS_DATA_DIRTY;

    return VOP_PUTPAGES(t, ap->a_m, ap->a_count, ap->a_sync, ap->a_rtvals,
			ap->a_offset);
#else /* NetBSD-style */
#if defined(__NetBSD__) && __NetBSD_Version__  >= 105250000 
    /* XXX should only walk over those pages that is requested */
    if (vp->v_type == VREG && ap->a_flags & PGO_CLEANIT) {
	struct uvm_object *uobj = &t->v_uobj;
	struct vm_page *pg;
	int dirty = 0;

	pg = TAILQ_FIRST(&uobj->memq);

	while (pg && !dirty) {
	    dirty = pmap_is_modified(pg) || (pg->flags & PG_CLEAN) == 0;
	    pg = TAILQ_NEXT(pg, listq);
	}	

	if (dirty)
	    xn->flags |= NNPFS_DATA_DIRTY;
    }

    return VOP_PUTPAGES(t, ap->a_offlo, ap->a_offhi, ap->a_flags);
#else
    xn->flags |= NNPFS_DATA_DIRTY;
    return VOP_PUTPAGES(t, ap->a_m, ap->a_count, ap->a_flags, ap->a_rtvals);
#endif
#endif /* HAVE_STRUCT_VOP_PUTPAGES_ARGS_A_SYNC */
}
#endif /* HAVE_VOP_PUTPAGES */

#ifdef HAVE_VOP_CMP
int
nnpfs_cmp(struct vnode * vp1, struct vnode * vp2)
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_cmp\n"));
    return EOPNOTSUPP;
}
#endif /* HAVE_VOP_CMP */

#ifdef HAVE_VOP_REALVP
int
nnpfs_realvp(struct vnode * vp,
	   struct vnode ** vpp)
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_realvp\n"));
    return EOPNOTSUPP;
}
#endif /* HAVE_VOP_REALVP */

#ifdef HAVE_VOP_CNTL
int
nnpfs_cntl(struct vnode * vp,
	 int cmd,
	 caddr_t idata,
	 caddr_t odata,
	 int iflag,
	 int oflag)
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_cntl\n"));
    return EOPNOTSUPP;
}
#endif /* HAVE_VOP_CNTL */

#ifdef HAVE_VOP_PRINT
int
nnpfs_print (struct vop_print_args *v)
{
    struct vop_print_args *ap = v;
    nnpfs_printnode_common (ap->a_vp);
    return 0;
}
#endif

#ifdef HAVE_VOP_ADVLOCK
int
nnpfs_advlock(struct vop_advlock_args *v)
{
    struct vop_advlock_args *ap = v;
#if defined(HAVE_KERNEL_LF_ADVLOCK) && !defined(__APPLE__)
    struct nnpfs_node *xn = VNODE_TO_XNODE(ap->a_vp);
 
    return (lf_advlock(&xn->lockf, xn->attr.va_size, ap->a_id, ap->a_op,
	    ap->a_fl, ap->a_flags));
#else
     return EOPNOTSUPP;
#endif
}
#endif /* HAVE_VOP_ADVOCK */

#ifdef HAVE_VOP_REVOKE
int
nnpfs_revoke(struct vop_revoke_args *v)
{
#if defined(HAVE_KERNEL_GENFS_REVOKE)
    return genfs_revoke (v);
#elif defined(HAVE_KERNEL_VOP_REVOKE)
    return vop_revoke (v);
#else
    return EOPNOTSUPP;
#endif
}
#endif /* HAVE_VOP_REVOKE */

#ifdef HAVE_VOP_PAGEIN
int
nnpfs_pagein(struct vop_pagein_args *ap)
{
#ifdef __APPLE__
    struct uio uio;
    struct iovec iov;
    int ret;

    kernel_upl_map(kernel_map, ap->a_pl, &iov.iov_base);
    iov.iov_base+=ap->a_pl_offset;
    iov.iov_len=ap->a_size;

    uio.uio_iov=&iov;
    uio.uio_iovcnt=1;
    uio.uio_offset=ap->a_f_offset;
    uio.uio_resid=ap->a_size;
    uio.uio_segflg=UIO_SYSSPACE; /* XXX what is it? */
    uio.uio_rw=UIO_READ;
    uio.uio_procp=nnpfs_curproc();

    ret = VOP_READ(ap->a_vp, &uio, 0, ap->a_cred);

    /* Zero out rest of last page if there wasn't enough data in the file */
    if (ret == 0 && uio.uio_resid > 0)
	bzero(iov.iov_base, uio.uio_resid);
    
    kernel_upl_unmap(kernel_map, ap->a_pl);

    if (ret) {
	kernel_upl_abort_range(ap->a_pl, ap->a_pl_offset, ap->a_size,
			       UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
    } else {
	kernel_upl_commit_range(ap->a_pl, ap->a_pl_offset, ap->a_size,
				UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY,
				UPL_GET_INTERNAL_PAGE_LIST(ap->a_pl));
    }

    return ret;
#else
#error pagein on non apple ?
#endif
}
  
#endif

#ifdef HAVE_VOP_PAGEOUT
int
nnpfs_pageout(struct vop_pageout_args *ap)
{
#ifdef __APPLE__
    struct uio uio;
    struct iovec iov;
    int ret;

    kernel_upl_map(kernel_map, ap->a_pl, &iov.iov_base);
    iov.iov_base+=ap->a_pl_offset;
    iov.iov_len=ap->a_size;

    uio.uio_iov=&iov;
    uio.uio_iovcnt=1;
    uio.uio_offset=ap->a_f_offset;
    uio.uio_resid=ap->a_size;
    uio.uio_segflg=UIO_SYSSPACE; /* XXX what is it? */
    uio.uio_rw=UIO_WRITE;
    uio.uio_procp=nnpfs_curproc();

    ret = VOP_WRITE(ap->a_vp, &uio, 0, ap->a_cred);

    kernel_upl_unmap(kernel_map, ap->a_pl);

    if (ret) {
	kernel_upl_abort_range(ap->a_pl, ap->a_pl_offset, ap->a_size,
			       UPL_ABORT_FREE_ON_EMPTY);
    } else {
	kernel_upl_commit_range(ap->a_pl, ap->a_pl_offset, ap->a_size,
				UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY,
				UPL_GET_INTERNAL_PAGE_LIST(ap->a_pl));
    }

    return ret;
#else
#error pageout on non apple ?
#endif
}
#endif

#ifdef HAVE_VOP_CREATEVOBJECT
int
nnpfs_createvobject(struct vop_createvobject_args *ap)
/*
struct vop_createvobject_args {
	struct vnode *vp;
	struct ucred *cred;
	struct proc *p;
};
 */
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_createvobject\n"));

    return vop_stdcreatevobject (ap);
}
#endif /* HAVE_VOP_CREATEVOBJECT */

#ifdef HAVE_VOP_DESTROYVOBJECT
int
nnpfs_destroyvobject(struct vop_destroyvobject_args *ap)
/*
struct vop_destroyvobject_args {
	struct vnode *vp;
};
 */
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_destroyvobject\n"));

    return vop_stddestroyvobject (ap);
}
#endif /* HAVE_VOP_DESTROYVOBJECT */

#ifdef HAVE_VOP_GETVOBJECT
int
nnpfs_getvobject(struct vop_getvobject_args *ap)
/*
struct vop_getvobject_args {
	struct vnode *vp;
	struct vm_object **objpp;
};
 */
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_getvobject\n"));

    return vop_stdgetvobject (ap);
}
#endif /* HAVE_VOP_GETVOBJECT */

#ifdef HAVE_VOP_PATHCONF
int
nnpfs_pathconf(struct vop_pathconf_args *ap)
/*
struct vop_pathconf_args {
        struct vnodeop_desc *a_desc;
        struct vnode *a_vp;
        int a_name;
};
*/
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_pathconf\n"));

#ifdef HAVE_KERNEL_VOP_STDPATHCONF
    return vop_stdpathconf(ap);
#else
    return EOPNOTSUPP;
#endif
}
#endif



vop_t **nnpfs_vnodeop_p;

int
nnpfs_eopnotsupp (struct vop_generic_args *ap)
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_eopnotsupp %s\n", ap->a_desc->vdesc_name));
    return EOPNOTSUPP;
}

int
nnpfs_returnzero (struct vop_generic_args *ap)
{
    NNPFSDEB(XDEBVNOPS, ("nnpfs_returnzero %s\n", ap->a_desc->vdesc_name));
    return 0;
}

void
nnpfs_pushdirty(struct vnode *vp, struct ucred *cred, d_thread_t *p)
{
#if defined(__NetBSD_Version__) && __NetBSD_Version__ >= 105280000
    VOP_PUTPAGES(vp, 0, 0, PGO_ALLPAGES|PGO_SYNCIO|PGO_CLEANIT);
#elif defined(__APPLE__)
    ubc_pushdirty(vp);
#endif
}



static struct vnodeopv_entry_desc nnpfs_vnodeop_entries[] = {
    {&vop_default_desc, (vop_t *) nnpfs_eopnotsupp},
#ifdef HAVE_VOP_LOOKUP
#ifdef HAVE_KERNEL_VFS_CACHE_LOOKUP
    {&vop_lookup_desc, (vop_t *) vfs_cache_lookup },
#else
    {&vop_lookup_desc, (vop_t *) nnpfs_lookup },
#endif
#endif
#ifdef HAVE_VOP_CACHEDLOOKUP
    {&vop_cachedlookup_desc, (vop_t *) nnpfs_cachedlookup },
#endif
#ifdef HAVE_VOP_OPEN
    {&vop_open_desc, (vop_t *) nnpfs_open },
#endif
#ifdef HAVE_VOP_FSYNC
    {&vop_fsync_desc, (vop_t *) nnpfs_fsync },
#endif
#ifdef HAVE_VOP_CLOSE
    {&vop_close_desc, (vop_t *) nnpfs_close },
#endif
#ifdef HAVE_VOP_READ
    {&vop_read_desc, (vop_t *) nnpfs_read },
#endif
#ifdef HAVE_VOP_WRITE
    {&vop_write_desc, (vop_t *) nnpfs_write },
#endif
#ifdef HAVE_VOP_MMAP
    {&vop_mmap_desc, (vop_t *) nnpfs_mmap },
#endif
#ifdef HAVE_VOP_BMAP
    {&vop_bmap_desc, (vop_t *) nnpfs_bmap },
#endif
#ifdef HAVE_VOP_IOCTL
    {&vop_ioctl_desc, (vop_t *) nnpfs_ioctl },
#endif
#ifdef HAVE_VOP_SELECT
    {&vop_select_desc, (vop_t *) nnpfs_select },
#endif
#ifdef HAVE_VOP_SEEK
    {&vop_seek_desc, (vop_t *) nnpfs_seek },
#endif
#ifdef HAVE_VOP_POLL
    {&vop_poll_desc, (vop_t *) nnpfs_poll },
#endif
#ifdef HAVE_VOP_GETATTR
    {&vop_getattr_desc, (vop_t *) nnpfs_getattr },
#endif
#ifdef HAVE_VOP_SETATTR
    {&vop_setattr_desc, (vop_t *) nnpfs_setattr },
#endif
#ifdef HAVE_VOP_ACCESS
    {&vop_access_desc, (vop_t *) nnpfs_access },
#endif
#ifdef HAVE_VOP_CREATE
    {&vop_create_desc, (vop_t *) nnpfs_create },
#endif
#ifdef HAVE_VOP_REMOVE
    {&vop_remove_desc, (vop_t *) nnpfs_remove },
#endif
#ifdef HAVE_VOP_LINK
    {&vop_link_desc, (vop_t *) nnpfs_link },
#endif
#ifdef HAVE_VOP_RENAME
    {&vop_rename_desc, (vop_t *) nnpfs_rename },
#endif
#ifdef HAVE_VOP_MKDIR
    {&vop_mkdir_desc, (vop_t *) nnpfs_mkdir },
#endif
#ifdef HAVE_VOP_RMDIR
    {&vop_rmdir_desc, (vop_t *) nnpfs_rmdir },
#endif
#ifdef HAVE_VOP_READDIR
    {&vop_readdir_desc, (vop_t *) nnpfs_readdir },
#endif
#ifdef HAVE_VOP_SYMLINK
    {&vop_symlink_desc, (vop_t *) nnpfs_symlink },
#endif
#ifdef HAVE_VOP_READLINK
    {&vop_readlink_desc, (vop_t *) nnpfs_readlink },
#endif
#ifdef HAVE_VOP_INACTIVE
    {&vop_inactive_desc, (vop_t *) nnpfs_inactive },
#endif
#ifdef HAVE_VOP_RECLAIM
    {&vop_reclaim_desc, (vop_t *) nnpfs_reclaim },
#endif
#ifdef HAVE_VOP_LOCK
    {&vop_lock_desc, (vop_t *) nnpfs_lock },
#endif
#ifdef HAVE_VOP_UNLOCK
    {&vop_unlock_desc, (vop_t *) nnpfs_unlock },
#endif
#ifdef HAVE_VOP_ISLOCKED
    {&vop_islocked_desc, (vop_t *) nnpfs_islocked },
#endif
#ifdef HAVE_VOP_ABORTOP
    {&vop_abortop_desc, (vop_t *) nnpfs_abortop },
#endif
#ifdef HAVE_VOP_GETPAGES
    {&vop_getpages_desc, (vop_t *) nnpfs_getpages },
#endif
#ifdef HAVE_VOP_PUTPAGES
    {&vop_putpages_desc, (vop_t *) nnpfs_putpages },
#endif
#ifdef HAVE_VOP_REVOKE
    {&vop_revoke_desc, (vop_t *) nnpfs_revoke },
#endif
#ifdef HAVE_VOP_PRINT
    {&vop_print_desc, (vop_t *) nnpfs_print}, 
#endif
#ifdef HAVE_VOP_ADVLOCK
    {&vop_advlock_desc, (vop_t *) nnpfs_advlock },
#endif
#ifdef HAVE_VOP_PAGEIN
    {&vop_pagein_desc, (vop_t *) nnpfs_pagein },
#endif
#ifdef HAVE_VOP_PAGEOUT
    {&vop_pageout_desc, (vop_t *) nnpfs_pageout },
#endif
#ifdef HAVE_VOP_CREATEVOBJECT
    {&vop_createvobject_desc, (vop_t *) nnpfs_createvobject },
#endif
#ifdef HAVE_VOP_DESTROYVOBJECT
    {&vop_destroyvobject_desc, (vop_t *) nnpfs_destroyvobject },
#endif
#ifdef HAVE_VOP_GETVOBJECT
    {&vop_getvobject_desc, (vop_t *) nnpfs_getvobject },
#endif
#ifdef HAVE_VOP_PATHCONF
    {&vop_pathconf_desc, (vop_t *) nnpfs_pathconf },
#endif
    {(struct vnodeop_desc *) NULL, (int (*) (void *)) NULL}
};

struct vnodeopv_desc nnpfs_vnodeop_opv_desc =
{&nnpfs_vnodeop_p, nnpfs_vnodeop_entries};

#ifdef VNODEOP_SET
VNODEOP_SET(nnpfs_vnodeop_opv_desc);
#endif
