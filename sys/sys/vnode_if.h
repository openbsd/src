/*
 * Warning: This file is generated automatically.
 * (Modifications made here may easily be lost!)
 *
 * Created from the file:
 *	OpenBSD: vnode_if.src,v 1.8 1998/08/08 03:53:30 csapuntz Exp 
 * by the script:
 *	OpenBSD: vnode_if.sh,v 1.3 1996/04/19 16:09:13 niklas Exp 
 */

/*
 * Copyright (c) 1992, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS AS IS'' AND
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
 */

extern struct vnodeop_desc vop_default_desc;

#include "systm.h"

struct vop_islocked_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern struct vnodeop_desc vop_islocked_desc;
static __inline int VOP_ISLOCKED __P((struct vnode *));
static __inline int VOP_ISLOCKED(vp)
	struct vnode *vp;
{
	struct vop_islocked_args a;
	a.a_desc = VDESC(vop_islocked);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_islocked), &a));
}

struct vop_lookup_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_lookup_desc;
static __inline int VOP_LOOKUP __P((struct vnode *, struct vnode **, 
    struct componentname *));
static __inline int VOP_LOOKUP(dvp, vpp, cnp)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	struct vop_lookup_args a;
	a.a_desc = VDESC(vop_lookup);
	a.a_dvp = dvp;
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_lookup), &a));
}

struct vop_create_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern struct vnodeop_desc vop_create_desc;
static __inline int VOP_CREATE __P((struct vnode *, struct vnode **, 
    struct componentname *, struct vattr *));
static __inline int VOP_CREATE(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct vop_create_args a;
	a.a_desc = VDESC(vop_create);
	a.a_dvp = dvp;
#ifdef DIAGNOSTIC
	if ((dvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(dvp)) panic("vop_create: dvp");
#endif
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	return (VCALL(dvp, VOFFSET(vop_create), &a));
}

struct vop_mknod_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern struct vnodeop_desc vop_mknod_desc;
static __inline int VOP_MKNOD __P((struct vnode *, struct vnode **, 
    struct componentname *, struct vattr *));
static __inline int VOP_MKNOD(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct vop_mknod_args a;
	a.a_desc = VDESC(vop_mknod);
	a.a_dvp = dvp;
#ifdef DIAGNOSTIC
	if ((dvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(dvp)) panic("vop_mknod: dvp");
#endif
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	return (VCALL(dvp, VOFFSET(vop_mknod), &a));
}

struct vop_open_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_open_desc;
static __inline int VOP_OPEN __P((struct vnode *, int, struct ucred *, 
    struct proc *));
static __inline int VOP_OPEN(vp, mode, cred, p)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_open_args a;
	a.a_desc = VDESC(vop_open);
	a.a_vp = vp;
	a.a_mode = mode;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_open), &a));
}

struct vop_close_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_fflag;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_close_desc;
static __inline int VOP_CLOSE __P((struct vnode *, int, struct ucred *, 
    struct proc *));
static __inline int VOP_CLOSE(vp, fflag, cred, p)
	struct vnode *vp;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_close_args a;
	a.a_desc = VDESC(vop_close);
	a.a_vp = vp;
	a.a_fflag = fflag;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_close), &a));
}

struct vop_access_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_access_desc;
static __inline int VOP_ACCESS __P((struct vnode *, int, struct ucred *, 
    struct proc *));
static __inline int VOP_ACCESS(vp, mode, cred, p)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_access_args a;
	a.a_desc = VDESC(vop_access);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_access: vp");
#endif
	a.a_mode = mode;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_access), &a));
}

struct vop_getattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vattr *a_vap;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_getattr_desc;
static __inline int VOP_GETATTR __P((struct vnode *, struct vattr *, 
    struct ucred *, struct proc *));
static __inline int VOP_GETATTR(vp, vap, cred, p)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_getattr_args a;
	a.a_desc = VDESC(vop_getattr);
	a.a_vp = vp;
	a.a_vap = vap;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_getattr), &a));
}

struct vop_setattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vattr *a_vap;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_setattr_desc;
static __inline int VOP_SETATTR __P((struct vnode *, struct vattr *, 
    struct ucred *, struct proc *));
static __inline int VOP_SETATTR(vp, vap, cred, p)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_setattr_args a;
	a.a_desc = VDESC(vop_setattr);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_setattr: vp");
#endif
	a.a_vap = vap;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_setattr), &a));
}

struct vop_read_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	int a_ioflag;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_read_desc;
static __inline int VOP_READ __P((struct vnode *, struct uio *, int, 
    struct ucred *));
static __inline int VOP_READ(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct vop_read_args a;
	a.a_desc = VDESC(vop_read);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_read: vp");
#endif
	a.a_uio = uio;
	a.a_ioflag = ioflag;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_read), &a));
}

struct vop_write_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	int a_ioflag;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_write_desc;
static __inline int VOP_WRITE __P((struct vnode *, struct uio *, int, 
    struct ucred *));
static __inline int VOP_WRITE(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct vop_write_args a;
	a.a_desc = VDESC(vop_write);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_write: vp");
#endif
	a.a_uio = uio;
	a.a_ioflag = ioflag;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_write), &a));
}

struct vop_lease_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct proc *a_p;
	struct ucred *a_cred;
	int a_flag;
};
extern struct vnodeop_desc vop_lease_desc;
static __inline int VOP_LEASE __P((struct vnode *, struct proc *, 
    struct ucred *, int));
static __inline int VOP_LEASE(vp, p, cred, flag)
	struct vnode *vp;
	struct proc *p;
	struct ucred *cred;
	int flag;
{
	struct vop_lease_args a;
	a.a_desc = VDESC(vop_lease);
	a.a_vp = vp;
	a.a_p = p;
	a.a_cred = cred;
	a.a_flag = flag;
	return (VCALL(vp, VOFFSET(vop_lease), &a));
}

struct vop_ioctl_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	u_long a_command;
	caddr_t a_data;
	int a_fflag;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_ioctl_desc;
static __inline int VOP_IOCTL __P((struct vnode *, u_long, caddr_t, int, 
    struct ucred *, struct proc *));
static __inline int VOP_IOCTL(vp, command, data, fflag, cred, p)
	struct vnode *vp;
	u_long command;
	caddr_t data;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_ioctl_args a;
	a.a_desc = VDESC(vop_ioctl);
	a.a_vp = vp;
	a.a_command = command;
	a.a_data = data;
	a.a_fflag = fflag;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_ioctl), &a));
}

struct vop_select_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_which;
	int a_fflags;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_select_desc;
static __inline int VOP_SELECT __P((struct vnode *, int, int, 
    struct ucred *, struct proc *));
static __inline int VOP_SELECT(vp, which, fflags, cred, p)
	struct vnode *vp;
	int which;
	int fflags;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_select_args a;
	a.a_desc = VDESC(vop_select);
	a.a_vp = vp;
	a.a_which = which;
	a.a_fflags = fflags;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_select), &a));
}

struct vop_revoke_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
};
extern struct vnodeop_desc vop_revoke_desc;
static __inline int VOP_REVOKE __P((struct vnode *, int));
static __inline int VOP_REVOKE(vp, flags)
	struct vnode *vp;
	int flags;
{
	struct vop_revoke_args a;
	a.a_desc = VDESC(vop_revoke);
	a.a_vp = vp;
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_revoke), &a));
}

struct vop_mmap_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_fflags;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_mmap_desc;
static __inline int VOP_MMAP __P((struct vnode *, int, struct ucred *, 
    struct proc *));
static __inline int VOP_MMAP(vp, fflags, cred, p)
	struct vnode *vp;
	int fflags;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_mmap_args a;
	a.a_desc = VDESC(vop_mmap);
	a.a_vp = vp;
	a.a_fflags = fflags;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_mmap), &a));
}

struct vop_fsync_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct ucred *a_cred;
	int a_waitfor;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_fsync_desc;
static __inline int VOP_FSYNC __P((struct vnode *, struct ucred *, int, 
    struct proc *));
static __inline int VOP_FSYNC(vp, cred, waitfor, p)
	struct vnode *vp;
	struct ucred *cred;
	int waitfor;
	struct proc *p;
{
	struct vop_fsync_args a;
	a.a_desc = VDESC(vop_fsync);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_fsync: vp");
#endif
	a.a_cred = cred;
	a.a_waitfor = waitfor;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_fsync), &a));
}

struct vop_seek_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_oldoff;
	off_t a_newoff;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_seek_desc;
static __inline int VOP_SEEK __P((struct vnode *, off_t, off_t, 
    struct ucred *));
static __inline int VOP_SEEK(vp, oldoff, newoff, cred)
	struct vnode *vp;
	off_t oldoff;
	off_t newoff;
	struct ucred *cred;
{
	struct vop_seek_args a;
	a.a_desc = VDESC(vop_seek);
	a.a_vp = vp;
	a.a_oldoff = oldoff;
	a.a_newoff = newoff;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_seek), &a));
}

struct vop_remove_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_remove_desc;
static __inline int VOP_REMOVE __P((struct vnode *, struct vnode *, 
    struct componentname *));
static __inline int VOP_REMOVE(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	struct vop_remove_args a;
	a.a_desc = VDESC(vop_remove);
	a.a_dvp = dvp;
#ifdef DIAGNOSTIC
	if ((dvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(dvp)) panic("vop_remove: dvp");
#endif
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_remove: vp");
#endif
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_remove), &a));
}

struct vop_link_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_link_desc;
static __inline int VOP_LINK __P((struct vnode *, struct vnode *, 
    struct componentname *));
static __inline int VOP_LINK(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	struct vop_link_args a;
	a.a_desc = VDESC(vop_link);
	a.a_dvp = dvp;
#ifdef DIAGNOSTIC
	if ((dvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(dvp)) panic("vop_link: dvp");
#endif
	a.a_vp = vp;
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_link), &a));
}

struct vop_rename_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_fdvp;
	struct vnode *a_fvp;
	struct componentname *a_fcnp;
	struct vnode *a_tdvp;
	struct vnode *a_tvp;
	struct componentname *a_tcnp;
};
extern struct vnodeop_desc vop_rename_desc;
static __inline int VOP_RENAME __P((struct vnode *, struct vnode *, 
    struct componentname *, struct vnode *, struct vnode *, 
    struct componentname *));
static __inline int VOP_RENAME(fdvp, fvp, fcnp, tdvp, tvp, tcnp)
	struct vnode *fdvp;
	struct vnode *fvp;
	struct componentname *fcnp;
	struct vnode *tdvp;
	struct vnode *tvp;
	struct componentname *tcnp;
{
	struct vop_rename_args a;
	a.a_desc = VDESC(vop_rename);
	a.a_fdvp = fdvp;
	a.a_fvp = fvp;
	a.a_fcnp = fcnp;
	a.a_tdvp = tdvp;
#ifdef DIAGNOSTIC
	if ((tdvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(tdvp)) panic("vop_rename: tdvp");
#endif
	a.a_tvp = tvp;
	a.a_tcnp = tcnp;
	return (VCALL(fdvp, VOFFSET(vop_rename), &a));
}

struct vop_mkdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern struct vnodeop_desc vop_mkdir_desc;
static __inline int VOP_MKDIR __P((struct vnode *, struct vnode **, 
    struct componentname *, struct vattr *));
static __inline int VOP_MKDIR(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct vop_mkdir_args a;
	a.a_desc = VDESC(vop_mkdir);
	a.a_dvp = dvp;
#ifdef DIAGNOSTIC
	if ((dvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(dvp)) panic("vop_mkdir: dvp");
#endif
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	return (VCALL(dvp, VOFFSET(vop_mkdir), &a));
}

struct vop_rmdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_rmdir_desc;
static __inline int VOP_RMDIR __P((struct vnode *, struct vnode *, 
    struct componentname *));
static __inline int VOP_RMDIR(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	struct vop_rmdir_args a;
	a.a_desc = VDESC(vop_rmdir);
	a.a_dvp = dvp;
#ifdef DIAGNOSTIC
	if ((dvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(dvp)) panic("vop_rmdir: dvp");
#endif
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_rmdir: vp");
#endif
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_rmdir), &a));
}

struct vop_symlink_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
	char *a_target;
};
extern struct vnodeop_desc vop_symlink_desc;
static __inline int VOP_SYMLINK __P((struct vnode *, struct vnode **, 
    struct componentname *, struct vattr *, char *));
static __inline int VOP_SYMLINK(dvp, vpp, cnp, vap, target)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
	char *target;
{
	struct vop_symlink_args a;
	a.a_desc = VDESC(vop_symlink);
	a.a_dvp = dvp;
#ifdef DIAGNOSTIC
	if ((dvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(dvp)) panic("vop_symlink: dvp");
#endif
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	a.a_target = target;
	return (VCALL(dvp, VOFFSET(vop_symlink), &a));
}

struct vop_readdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
	int *a_eofflag;
	int *a_ncookies;
	u_long **a_cookies;
};
extern struct vnodeop_desc vop_readdir_desc;
static __inline int VOP_READDIR __P((struct vnode *, struct uio *, 
    struct ucred *, int *, int *, u_long **));
static __inline int VOP_READDIR(vp, uio, cred, eofflag, ncookies, cookies)
	struct vnode *vp;
	struct uio *uio;
	struct ucred *cred;
	int *eofflag;
	int *ncookies;
	u_long **cookies;
{
	struct vop_readdir_args a;
	a.a_desc = VDESC(vop_readdir);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_readdir: vp");
#endif
	a.a_uio = uio;
	a.a_cred = cred;
	a.a_eofflag = eofflag;
	a.a_ncookies = ncookies;
	a.a_cookies = cookies;
	return (VCALL(vp, VOFFSET(vop_readdir), &a));
}

struct vop_readlink_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_readlink_desc;
static __inline int VOP_READLINK __P((struct vnode *, struct uio *, 
    struct ucred *));
static __inline int VOP_READLINK(vp, uio, cred)
	struct vnode *vp;
	struct uio *uio;
	struct ucred *cred;
{
	struct vop_readlink_args a;
	a.a_desc = VDESC(vop_readlink);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_readlink: vp");
#endif
	a.a_uio = uio;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_readlink), &a));
}

struct vop_abortop_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_abortop_desc;
static __inline int VOP_ABORTOP __P((struct vnode *, struct componentname *));
static __inline int VOP_ABORTOP(dvp, cnp)
	struct vnode *dvp;
	struct componentname *cnp;
{
	struct vop_abortop_args a;
	a.a_desc = VDESC(vop_abortop);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_abortop), &a));
}

struct vop_inactive_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_inactive_desc;
static __inline int VOP_INACTIVE __P((struct vnode *, struct proc *));
static __inline int VOP_INACTIVE(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	struct vop_inactive_args a;
	a.a_desc = VDESC(vop_inactive);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_inactive: vp");
#endif
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_inactive), &a));
}

struct vop_reclaim_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_reclaim_desc;
static __inline int VOP_RECLAIM __P((struct vnode *, struct proc *));
static __inline int VOP_RECLAIM(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	struct vop_reclaim_args a;
	a.a_desc = VDESC(vop_reclaim);
	a.a_vp = vp;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_reclaim), &a));
}

struct vop_lock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_lock_desc;
static __inline int VOP_LOCK __P((struct vnode *, int, struct proc *));
static __inline int VOP_LOCK(vp, flags, p)
	struct vnode *vp;
	int flags;
	struct proc *p;
{
	struct vop_lock_args a;
	a.a_desc = VDESC(vop_lock);
	a.a_vp = vp;
	a.a_flags = flags;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_lock), &a));
}

struct vop_unlock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_unlock_desc;
static __inline int VOP_UNLOCK __P((struct vnode *, int, struct proc *));
static __inline int VOP_UNLOCK(vp, flags, p)
	struct vnode *vp;
	int flags;
	struct proc *p;
{
	struct vop_unlock_args a;
	a.a_desc = VDESC(vop_unlock);
	a.a_vp = vp;
	a.a_flags = flags;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_unlock), &a));
}

struct vop_bmap_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	daddr_t a_bn;
	struct vnode **a_vpp;
	daddr_t *a_bnp;
	int *a_runp;
};
extern struct vnodeop_desc vop_bmap_desc;
static __inline int VOP_BMAP __P((struct vnode *, daddr_t, struct vnode **, 
    daddr_t *, int *));
static __inline int VOP_BMAP(vp, bn, vpp, bnp, runp)
	struct vnode *vp;
	daddr_t bn;
	struct vnode **vpp;
	daddr_t *bnp;
	int *runp;
{
	struct vop_bmap_args a;
	a.a_desc = VDESC(vop_bmap);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_bmap: vp");
#endif
	a.a_bn = bn;
	a.a_vpp = vpp;
	a.a_bnp = bnp;
	a.a_runp = runp;
	return (VCALL(vp, VOFFSET(vop_bmap), &a));
}

struct vop_print_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern struct vnodeop_desc vop_print_desc;
static __inline int VOP_PRINT __P((struct vnode *));
static __inline int VOP_PRINT(vp)
	struct vnode *vp;
{
	struct vop_print_args a;
	a.a_desc = VDESC(vop_print);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_print), &a));
}

struct vop_pathconf_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_name;
	register_t *a_retval;
};
extern struct vnodeop_desc vop_pathconf_desc;
static __inline int VOP_PATHCONF __P((struct vnode *, int, register_t *));
static __inline int VOP_PATHCONF(vp, name, retval)
	struct vnode *vp;
	int name;
	register_t *retval;
{
	struct vop_pathconf_args a;
	a.a_desc = VDESC(vop_pathconf);
	a.a_vp = vp;
	a.a_name = name;
	a.a_retval = retval;
	return (VCALL(vp, VOFFSET(vop_pathconf), &a));
}

struct vop_advlock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	caddr_t a_id;
	int a_op;
	struct flock *a_fl;
	int a_flags;
};
extern struct vnodeop_desc vop_advlock_desc;
static __inline int VOP_ADVLOCK __P((struct vnode *, caddr_t, int, 
    struct flock *, int));
static __inline int VOP_ADVLOCK(vp, id, op, fl, flags)
	struct vnode *vp;
	caddr_t id;
	int op;
	struct flock *fl;
	int flags;
{
	struct vop_advlock_args a;
	a.a_desc = VDESC(vop_advlock);
	a.a_vp = vp;
	a.a_id = id;
	a.a_op = op;
	a.a_fl = fl;
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_advlock), &a));
}

struct vop_blkatoff_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_offset;
	char **a_res;
	struct buf **a_bpp;
};
extern struct vnodeop_desc vop_blkatoff_desc;
static __inline int VOP_BLKATOFF __P((struct vnode *, off_t, char **, 
    struct buf **));
static __inline int VOP_BLKATOFF(vp, offset, res, bpp)
	struct vnode *vp;
	off_t offset;
	char **res;
	struct buf **bpp;
{
	struct vop_blkatoff_args a;
	a.a_desc = VDESC(vop_blkatoff);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_blkatoff: vp");
#endif
	a.a_offset = offset;
	a.a_res = res;
	a.a_bpp = bpp;
	return (VCALL(vp, VOFFSET(vop_blkatoff), &a));
}

struct vop_valloc_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_pvp;
	int a_mode;
	struct ucred *a_cred;
	struct vnode **a_vpp;
};
extern struct vnodeop_desc vop_valloc_desc;
static __inline int VOP_VALLOC __P((struct vnode *, int, struct ucred *, 
    struct vnode **));
static __inline int VOP_VALLOC(pvp, mode, cred, vpp)
	struct vnode *pvp;
	int mode;
	struct ucred *cred;
	struct vnode **vpp;
{
	struct vop_valloc_args a;
	a.a_desc = VDESC(vop_valloc);
	a.a_pvp = pvp;
#ifdef DIAGNOSTIC
	if ((pvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(pvp)) panic("vop_valloc: pvp");
#endif
	a.a_mode = mode;
	a.a_cred = cred;
	a.a_vpp = vpp;
	return (VCALL(pvp, VOFFSET(vop_valloc), &a));
}

struct vop_balloc_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_startoffset;
	int a_size;
	struct ucred *a_cred;
	int a_flags;
	struct buf **a_bpp;
};
extern struct vnodeop_desc vop_balloc_desc;
static __inline int VOP_BALLOC __P((struct vnode *, off_t, int, 
    struct ucred *, int, struct buf **));
static __inline int VOP_BALLOC(vp, startoffset, size, cred, flags, bpp)
	struct vnode *vp;
	off_t startoffset;
	int size;
	struct ucred *cred;
	int flags;
	struct buf **bpp;
{
	struct vop_balloc_args a;
	a.a_desc = VDESC(vop_balloc);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_balloc: vp");
#endif
	a.a_startoffset = startoffset;
	a.a_size = size;
	a.a_cred = cred;
	a.a_flags = flags;
	a.a_bpp = bpp;
	return (VCALL(vp, VOFFSET(vop_balloc), &a));
}

struct vop_reallocblks_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct cluster_save *a_buflist;
};
extern struct vnodeop_desc vop_reallocblks_desc;
static __inline int VOP_REALLOCBLKS __P((struct vnode *, 
    struct cluster_save *));
static __inline int VOP_REALLOCBLKS(vp, buflist)
	struct vnode *vp;
	struct cluster_save *buflist;
{
	struct vop_reallocblks_args a;
	a.a_desc = VDESC(vop_reallocblks);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_reallocblks: vp");
#endif
	a.a_buflist = buflist;
	return (VCALL(vp, VOFFSET(vop_reallocblks), &a));
}

struct vop_vfree_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_pvp;
	ino_t a_ino;
	int a_mode;
};
extern struct vnodeop_desc vop_vfree_desc;
static __inline int VOP_VFREE __P((struct vnode *, ino_t, int));
static __inline int VOP_VFREE(pvp, ino, mode)
	struct vnode *pvp;
	ino_t ino;
	int mode;
{
	struct vop_vfree_args a;
	a.a_desc = VDESC(vop_vfree);
	a.a_pvp = pvp;
#ifdef DIAGNOSTIC
	if ((pvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(pvp)) panic("vop_vfree: pvp");
#endif
	a.a_ino = ino;
	a.a_mode = mode;
	return (VCALL(pvp, VOFFSET(vop_vfree), &a));
}

struct vop_truncate_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_length;
	int a_flags;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_truncate_desc;
static __inline int VOP_TRUNCATE __P((struct vnode *, off_t, int, 
    struct ucred *, struct proc *));
static __inline int VOP_TRUNCATE(vp, length, flags, cred, p)
	struct vnode *vp;
	off_t length;
	int flags;
	struct ucred *cred;
	struct proc *p;
{
	struct vop_truncate_args a;
	a.a_desc = VDESC(vop_truncate);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_truncate: vp");
#endif
	a.a_length = length;
	a.a_flags = flags;
	a.a_cred = cred;
	a.a_p = p;
	return (VCALL(vp, VOFFSET(vop_truncate), &a));
}

struct vop_update_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct timespec *a_access;
	struct timespec *a_modify;
	int a_waitfor;
};
extern struct vnodeop_desc vop_update_desc;
static __inline int VOP_UPDATE __P((struct vnode *, struct timespec *, 
    struct timespec *, int));
static __inline int VOP_UPDATE(vp, access, modify, waitfor)
	struct vnode *vp;
	struct timespec *access;
	struct timespec *modify;
	int waitfor;
{
	struct vop_update_args a;
	a.a_desc = VDESC(vop_update);
	a.a_vp = vp;
#ifdef DIAGNOSTIC
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp)) panic("vop_update: vp");
#endif
	a.a_access = access;
	a.a_modify = modify;
	a.a_waitfor = waitfor;
	return (VCALL(vp, VOFFSET(vop_update), &a));
}

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
};
extern struct vnodeop_desc vop_whiteout_desc;
static __inline int VOP_WHITEOUT __P((struct vnode *, 
    struct componentname *, int));
static __inline int VOP_WHITEOUT(dvp, cnp, flags)
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
{
	struct vop_whiteout_args a;
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
#ifdef DIAGNOSTIC
	if ((dvp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(dvp)) panic("vop_whiteout: dvp");
#endif
	a.a_cnp = cnp;
	a.a_flags = flags;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}

/* Special cases: */
#include <sys/buf.h>

struct vop_strategy_args {
	struct vnodeop_desc *a_desc;
	struct buf *a_bp;
};
extern struct vnodeop_desc vop_strategy_desc;
static __inline int VOP_STRATEGY __P((struct buf *));
static __inline int VOP_STRATEGY(bp)
	struct buf *bp;
{
	struct vop_strategy_args a;
	a.a_desc = VDESC(vop_strategy);
	a.a_bp = bp;
	return (VCALL(bp->b_vp, VOFFSET(vop_strategy), &a));
}

struct vop_bwrite_args {
	struct vnodeop_desc *a_desc;
	struct buf *a_bp;
};
extern struct vnodeop_desc vop_bwrite_desc;
static __inline int VOP_BWRITE __P((struct buf *));
static __inline int VOP_BWRITE(bp)
	struct buf *bp;
{
	struct vop_bwrite_args a;
	a.a_desc = VDESC(vop_bwrite);
	a.a_bp = bp;
	return (VCALL(bp->b_vp, VOFFSET(vop_bwrite), &a));
}

/* End of special cases. */
