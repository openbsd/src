/*
 * Warning: This file is generated automatically.
 * (Modifications made here may easily be lost!)
 *
 * Created from the file:
 *	OpenBSD: vnode_if.src,v 1.19 2002/02/22 20:37:45 drahn Exp 
 * by the script:
 *	OpenBSD: vnode_if.sh,v 1.8 2001/02/26 17:34:18 art Exp 
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
int VOP_ISLOCKED __P((struct vnode *));

struct vop_lookup_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_lookup_desc;
int VOP_LOOKUP __P((struct vnode *, struct vnode **, struct componentname *));

struct vop_create_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern struct vnodeop_desc vop_create_desc;
int VOP_CREATE __P((struct vnode *, struct vnode **, struct componentname *, 
    struct vattr *));

struct vop_mknod_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern struct vnodeop_desc vop_mknod_desc;
int VOP_MKNOD __P((struct vnode *, struct vnode **, struct componentname *, 
    struct vattr *));

struct vop_open_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_open_desc;
int VOP_OPEN __P((struct vnode *, int, struct ucred *, struct proc *));

struct vop_close_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_fflag;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_close_desc;
int VOP_CLOSE __P((struct vnode *, int, struct ucred *, struct proc *));

struct vop_access_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_access_desc;
int VOP_ACCESS __P((struct vnode *, int, struct ucred *, struct proc *));

struct vop_getattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vattr *a_vap;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_getattr_desc;
int VOP_GETATTR __P((struct vnode *, struct vattr *, struct ucred *, 
    struct proc *));

struct vop_setattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vattr *a_vap;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_setattr_desc;
int VOP_SETATTR __P((struct vnode *, struct vattr *, struct ucred *, 
    struct proc *));

struct vop_read_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	int a_ioflag;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_read_desc;
int VOP_READ __P((struct vnode *, struct uio *, int, struct ucred *));

struct vop_write_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	int a_ioflag;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_write_desc;
int VOP_WRITE __P((struct vnode *, struct uio *, int, struct ucred *));

struct vop_lease_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct proc *a_p;
	struct ucred *a_cred;
	int a_flag;
};
extern struct vnodeop_desc vop_lease_desc;
int VOP_LEASE __P((struct vnode *, struct proc *, struct ucred *, int));

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
int VOP_IOCTL __P((struct vnode *, u_long, caddr_t, int, struct ucred *, 
    struct proc *));

struct vop_select_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_which;
	int a_fflags;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_select_desc;
int VOP_SELECT __P((struct vnode *, int, int, struct ucred *, struct proc *));

struct vop_kqfilter_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct knote *a_kn;
};
extern struct vnodeop_desc vop_kqfilter_desc;
int VOP_KQFILTER __P((struct vnode *, struct knote *));

struct vop_revoke_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
};
extern struct vnodeop_desc vop_revoke_desc;
int VOP_REVOKE __P((struct vnode *, int));

struct vop_fsync_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct ucred *a_cred;
	int a_waitfor;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_fsync_desc;
int VOP_FSYNC __P((struct vnode *, struct ucred *, int, struct proc *));

struct vop_remove_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_remove_desc;
int VOP_REMOVE __P((struct vnode *, struct vnode *, struct componentname *));

struct vop_link_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_link_desc;
int VOP_LINK __P((struct vnode *, struct vnode *, struct componentname *));

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
int VOP_RENAME __P((struct vnode *, struct vnode *, struct componentname *, 
    struct vnode *, struct vnode *, struct componentname *));

struct vop_mkdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern struct vnodeop_desc vop_mkdir_desc;
int VOP_MKDIR __P((struct vnode *, struct vnode **, struct componentname *, 
    struct vattr *));

struct vop_rmdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_rmdir_desc;
int VOP_RMDIR __P((struct vnode *, struct vnode *, struct componentname *));

struct vop_symlink_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
	char *a_target;
};
extern struct vnodeop_desc vop_symlink_desc;
int VOP_SYMLINK __P((struct vnode *, struct vnode **, 
    struct componentname *, struct vattr *, char *));

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
int VOP_READDIR __P((struct vnode *, struct uio *, struct ucred *, int *, 
    int *, u_long **));

struct vop_readlink_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
};
extern struct vnodeop_desc vop_readlink_desc;
int VOP_READLINK __P((struct vnode *, struct uio *, struct ucred *));

struct vop_abortop_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
};
extern struct vnodeop_desc vop_abortop_desc;
int VOP_ABORTOP __P((struct vnode *, struct componentname *));

struct vop_inactive_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_inactive_desc;
int VOP_INACTIVE __P((struct vnode *, struct proc *));

struct vop_reclaim_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_reclaim_desc;
int VOP_RECLAIM __P((struct vnode *, struct proc *));

struct vop_lock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_lock_desc;
int VOP_LOCK __P((struct vnode *, int, struct proc *));

struct vop_unlock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_unlock_desc;
int VOP_UNLOCK __P((struct vnode *, int, struct proc *));

struct vop_bmap_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	daddr_t a_bn;
	struct vnode **a_vpp;
	daddr_t *a_bnp;
	int *a_runp;
};
extern struct vnodeop_desc vop_bmap_desc;
int VOP_BMAP __P((struct vnode *, daddr_t, struct vnode **, daddr_t *, int *));

struct vop_print_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern struct vnodeop_desc vop_print_desc;
int VOP_PRINT __P((struct vnode *));

struct vop_pathconf_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_name;
	register_t *a_retval;
};
extern struct vnodeop_desc vop_pathconf_desc;
int VOP_PATHCONF __P((struct vnode *, int, register_t *));

struct vop_advlock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	caddr_t a_id;
	int a_op;
	struct flock *a_fl;
	int a_flags;
};
extern struct vnodeop_desc vop_advlock_desc;
int VOP_ADVLOCK __P((struct vnode *, caddr_t, int, struct flock *, int));

struct vop_reallocblks_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct cluster_save *a_buflist;
};
extern struct vnodeop_desc vop_reallocblks_desc;
int VOP_REALLOCBLKS __P((struct vnode *, struct cluster_save *));

struct vop_whiteout_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
};
extern struct vnodeop_desc vop_whiteout_desc;
int VOP_WHITEOUT __P((struct vnode *, struct componentname *, int));

struct vop_getextattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_attrnamespace;
	const char *a_name;
	struct uio *a_uio;
	size_t *a_size;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_getextattr_desc;
int VOP_GETEXTATTR __P((struct vnode *, int, const char *, struct uio *, 
    size_t *, struct ucred *, struct proc *));

struct vop_setextattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_attrnamespace;
	const char *a_name;
	struct uio *a_uio;
	struct ucred *a_cred;
	struct proc *a_p;
};
extern struct vnodeop_desc vop_setextattr_desc;
int VOP_SETEXTATTR __P((struct vnode *, int, const char *, struct uio *, 
    struct ucred *, struct proc *));

/* Special cases: */
#include <sys/buf.h>

struct vop_strategy_args {
	struct vnodeop_desc *a_desc;
	struct buf *a_bp;
};
extern struct vnodeop_desc vop_strategy_desc;
int VOP_STRATEGY __P((struct buf *));

struct vop_bwrite_args {
	struct vnodeop_desc *a_desc;
	struct buf *a_bp;
};
extern struct vnodeop_desc vop_bwrite_desc;
int VOP_BWRITE __P((struct buf *));

/* End of special cases. */
