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

/* $arla: nnpfs_vnodeops.h,v 1.25 2002/09/07 10:46:12 lha Exp $ */

#ifndef _nnpfs_vnodeops_h
#define _nnpfs_vnodeops_h

/*
 * nnpfs_vfs_readlock
 * nnpfs_vfs_writelock
 * nnpfs_vfs_unlock
 */

#ifdef __osf__			/* XXX - what about VN_LOCK? */

#define nnpfs_vfs_readlock(vp, proc) vref((vp))
#define nnpfs_vfs_writelock(vp, proc) vref((vp))
#define nnpfs_vfs_unlock(vp, proc) vrele((vp))

/* XXX - should this do anything? */

#define nnpfs_vfs_vn_lock(vp, flags, proc) (0)

#elif defined(HAVE_TWO_ARGUMENT_VOP_LOCK)

#define nnpfs_vfs_readlock(vp, proc) vn_lock((vp), LK_SHARED | LK_RETRY)
#define nnpfs_vfs_writelock(vp, proc) vn_lock((vp), LK_EXCLUSIVE | LK_RETRY)
#define nnpfs_vfs_unlock(vp, proc) VOP_UNLOCK((vp), 0)
#define nnpfs_vfs_vn_lock(vp, flags, proc) vn_lock((vp), (flags))

#elif defined(HAVE_THREE_ARGUMENT_VOP_LOCK)

#define nnpfs_vfs_readlock(vp, proc) vn_lock((vp), LK_SHARED | LK_RETRY, (proc))
#define nnpfs_vfs_writelock(vp, proc) vn_lock((vp), LK_EXCLUSIVE | LK_RETRY, (proc))
#define nnpfs_vfs_unlock(vp, proc) VOP_UNLOCK((vp), 0, (proc))
#define nnpfs_vfs_vn_lock(vp, flags, proc) vn_lock((vp), (flags), (proc))

#elif defined(HAVE_ONE_ARGUMENT_VOP_LOCK)

#define nnpfs_vfs_readlock(vp, proc) VOP_LOCK((vp))
#define nnpfs_vfs_writelock(vp, proc) VOP_LOCK((vp))
#define nnpfs_vfs_unlock(vp, proc) VOP_UNLOCK((vp))

/* XXX - should this do anything? */

#define nnpfs_vfs_vn_lock(vp, flags, proc) (0)

#else

#error what kind of VOP_LOCK?

#endif

int
nnpfs_open_valid(struct vnode *vp, struct ucred *cred, d_thread_t *p,
	       u_int tok);

int
nnpfs_attr_valid(struct vnode *vp, struct ucred *cred, d_thread_t *p,
	       u_int tok);

int
nnpfs_fetch_rights(struct vnode *vp, struct ucred *cred, d_thread_t *p);

int
nnpfs_data_valid(struct vnode *vp, struct ucred *cred, d_thread_t *p,
	       u_int tok, uint32_t offset);

int
nnpfs_open_common(struct vnode *vp,
		int mode,
		struct ucred *cred,
		d_thread_t *p);

int
nnpfs_fsync_common(struct vnode *vp, struct ucred *cred,
		 int waitfor, d_thread_t *proc);

int
nnpfs_close_common(struct vnode *vp, int fflag,
		 d_thread_t *proc, struct ucred *cred);

int
nnpfs_read_common(struct vnode *vp, struct uio *uio, int ioflag,
		struct ucred *cred);

int
nnpfs_write_common(struct vnode *vp, struct uio *uiop, int ioflag,
		 struct ucred *cred);

int
nnpfs_getattr_common(struct vnode *vp, struct vattr *vap,
		   struct ucred *cred, d_thread_t *p);

int
nnpfs_setattr_common(struct vnode *vp, struct vattr *vap,
		   struct ucred *cred, d_thread_t *p);

int
nnpfs_access_common(struct vnode *vp, int mode,
		  struct ucred *cred, d_thread_t *p);

int
nnpfs_lookup_common(struct vnode *dvp, 
		  nnpfs_componentname *cnp, 
		  struct vnode **vpp);

int
nnpfs_create_common(struct vnode *dvp,
		  const char *name,
		  struct vattr *vap, 
		  struct ucred *cred,
		  d_thread_t *p);

int
nnpfs_remove_common(struct vnode *dvp, 
		  struct vnode *vp, 
		  const char *name,
		  struct ucred *cred,
		  d_thread_t *p);

int
nnpfs_rename_common(struct vnode *fdvp, 
		  struct vnode *fvp,
		  const char *fname,
		  struct vnode *tdvp,
		  struct vnode *tvp,
		  const char *tname,
		  struct ucred *cred,
		  d_thread_t *p);

int
nnpfs_mkdir_common(struct vnode *dvp, 
		 const char *name,
		 struct vattr *vap, 
		 struct ucred *cred,
		 d_thread_t *p);

int
nnpfs_rmdir_common(struct vnode *dvp,
		 struct vnode *vp,
		 const char *name,
		 struct ucred *cred,
		 d_thread_t *p);

int
nnpfs_readdir_common(struct vnode *vp, 
		   struct uio *uiop, 
		   struct ucred *cred,
		   d_thread_t *p,
		   int *eofflag);

int
nnpfs_link_common(struct vnode *dvp, 
		struct vnode *vp, 
		const char *name,
		struct ucred *cred,
		d_thread_t *p);

int
nnpfs_symlink_common(struct vnode *dvp,
		   struct vnode **vpp,
		   nnpfs_componentname *cnp,
		   struct vattr *vap,
		   char *target);

int
nnpfs_readlink_common(struct vnode *vp, struct uio *uiop, struct ucred *cred);

int
nnpfs_inactive_common(struct vnode *vp, d_thread_t *p);

int
nnpfs_reclaim_common(struct vnode *vp);

int
nnpfs_eopnotsupp (struct vop_generic_args *ap);

int
nnpfs_returnzero (struct vop_generic_args *ap);

void
nnpfs_printnode_common (struct vnode *vp);

size_t
nnpfs_uio_end_length (struct uio *uio);


#endif /* _nnpfs_vnodeops_h */
