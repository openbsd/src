/* $OpenBSD: xfs_vnodeops.h,v 1.2 2000/03/03 00:54:59 todd Exp $ */
/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 *
 * 4. Neither the name of the Institute nor the names of its contributors
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


#ifndef _xfs_vnodeops_h
#define _xfs_vnodeops_h

/*
 * xfs_vfs_readlock
 * xfs_vfs_writelock
 * xfs_vfs_unlock
 */

#ifdef __osf__			/* XXX - what about VN_LOCK? */

#define xfs_vfs_readlock(vp, proc) VREF((vp))
#define xfs_vfs_writelock(vp, proc) VREF((vp))
#define xfs_vfs_unlock(vp, proc) vrele((vp))

#elif defined(HAVE_TWO_ARGUMENT_VOP_LOCK)

#define xfs_vfs_readlock(vp, proc) vn_lock((vp), LK_SHARED | LK_RETRY)
#define xfs_vfs_writelock(vp, proc) vn_lock((vp), LK_EXCLUSIVE | LK_RETRY)
#define xfs_vfs_unlock(vp, proc) VOP_UNLOCK((vp), 0)

#elif defined(HAVE_THREE_ARGUMENT_VOP_LOCK)

#define xfs_vfs_readlock(vp, proc) vn_lock((vp), LK_SHARED | LK_RETRY, (proc))
#define xfs_vfs_writelock(vp, proc) vn_lock((vp), LK_EXCLUSIVE | LK_RETRY, (proc))
#define xfs_vfs_unlock(vp, proc) VOP_UNLOCK((vp), 0, (proc))

#else

#define xfs_vfs_readlock(vp, proc) VOP_LOCK((vp))
#define xfs_vfs_writelock(vp, proc) VOP_LOCK((vp))
#define xfs_vfs_unlock(vp, proc) VOP_UNLOCK((vp))

#endif

int
xfs_open_valid(struct vnode * vp, struct ucred * cred, u_int tok);

int
xfs_attr_valid(struct vnode * vp, struct ucred * cred, u_int tok);

int
xfs_fetch_rights(struct vnode * vp, struct ucred * cred);

int
xfs_data_valid(struct vnode * vp, struct ucred * cred, u_int tok);

int
xfs_fsync_common(struct vnode *vp, struct ucred *cred,
		 int waitfor, struct proc *proc);

int
xfs_close_common(struct vnode *vp, int fflag,
		 struct proc *proc, struct ucred *cred);

int
xfs_read_common(struct vnode *vp, struct uio *uio, int ioflag, struct ucred *cred);

int
xfs_write_common(struct vnode *vp, struct uio *uiop, int ioflag, struct ucred *cred);

int
xfs_getattr_common(struct vnode *vp, struct vattr *vap, struct ucred *cred);

int
xfs_setattr_common(struct vnode *vp, struct vattr *vap, struct ucred *cred);

int
xfs_access_common(struct vnode *vp, int mode, struct ucred *cred);

int
xfs_lookup_common(struct vnode *dvp, 
		  xfs_componentname *cnp, 
		  struct vnode **vpp);

int
xfs_lookup_name(struct vnode *dvp, 
		const char *name,
		struct proc *proc,
		struct ucred *cred,
		struct vnode **vpp);

int
xfs_create_common(struct vnode *dvp,
		  const char *name,
		  struct vattr *vap, 
		  struct ucred *cred);

int
xfs_remove_common(struct vnode *dvp, 
		  struct vnode *vp, 
		  const char *name,
		  struct ucred *cred);

int
xfs_rename_common(struct vnode *fdvp, 
		  struct vnode *fvp,
		  const char *fname,
		  struct vnode *tdvp,
		  struct vnode *tvp,
		  const char *tname,
		  struct ucred *cred);

int
xfs_mkdir_common(struct vnode *dvp, 
		 const char *name,
		 struct vattr *vap, 
		 struct ucred *cred);

int
xfs_rmdir_common(struct vnode *dvp,
		 struct vnode *vp,
		 const char *name,
		 struct ucred *cred);

int
xfs_readdir_common(struct vnode *vp, 
		   struct uio *uiop, 
		   struct ucred *cred,
		   int *eofflag);

int
xfs_link_common(struct vnode *dvp, 
		struct vnode *vp, 
		const char *name,
		struct ucred *cred);

int
xfs_symlink_common(struct vnode *dvp,
		   struct vnode **vpp,
		   const char *name,
		   struct proc *proc,
		   struct ucred *cred,
		   struct vattr *vap,
		   char *target);

int
xfs_readlink_common(struct vnode *vp, struct uio *uiop, struct ucred *cred);

int
xfs_inactive_common(struct vnode *vp, struct proc *p);

int
xfs_reclaim_common(struct vnode *vp);

int
xfs_eopnotsupp (void *);

int
xfs_returnzero (void *v);

void
xfs_printnode_common (struct vnode *vp);

#endif /* _xfs_vnodeops_h */
