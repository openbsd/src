/*	$OpenBSD: xfs_vnodeops-common.c,v 1.3 2000/03/03 00:54:59 todd Exp $	*/

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

/*
 * XFS operations.
 */

#include <xfs/xfs_locl.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_common.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_deb.h>
#include <xfs/xfs_syscalls.h>
#include <xfs/xfs_vnodeops.h>

RCSID("$OpenBSD: xfs_vnodeops-common.c,v 1.3 2000/03/03 00:54:59 todd Exp $");

int
xfs_open_valid(struct vnode * vp, struct ucred * cred, u_int tok)
{
    struct xfs *xfsp = XFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;

    XFSDEB(XDEBVFOPS, ("xfs_open_valid\n"));

    do {
	if (!XFS_TOKEN_GOT(xn, tok)) {
	    struct xfs_message_open msg;

	    msg.header.opcode = XFS_MSG_OPEN;
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	    msg.handle = xn->handle;
	    msg.tokens = tok;
	    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	    if (error == 0)
		error = ((struct xfs_message_wakeup *) & msg)->error;
	} else {
	    goto done;
	}
    } while (error == 0);

done:
    XFSDEB(XDEBVFOPS, ("xfs_open_valid: error = %d\n", error));

    return error;
}

int
xfs_attr_valid(struct vnode * vp, struct ucred * cred, u_int tok)
{
    struct xfs *xfsp = XFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;
    pag_t pag = xfs_get_pag(cred);

    do {
	if (!XFS_TOKEN_GOT(xn, tok)) {
	    struct xfs_message_getattr msg;

	    msg.header.opcode = XFS_MSG_GETATTR;
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = pag;
	    msg.handle = xn->handle;
	    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	    if (error == 0)
		error = ((struct xfs_message_wakeup *) & msg)->error;
	} else {
	    goto done;
	}
    } while (error == 0);

done:
    return error;
}

int
xfs_fetch_rights(struct vnode * vp, struct ucred * cred)
{
    struct xfs *xfsp = XFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;

    pag_t pag = xfs_get_pag(cred);

    do {
	if (!xfs_has_pag(xn, pag)) {
	    struct xfs_message_getattr msg;

	    msg.header.opcode = XFS_MSG_GETATTR;
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = pag;
	    msg.handle = xn->handle;
	    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	    if (error == 0)
		error = ((struct xfs_message_wakeup *) & msg)->error;
	} else {
	    goto done;
	}
    } while (error == 0);

done:

    return error;
}

int
xfs_data_valid(struct vnode * vp, struct ucred * cred, u_int tok)
{
    struct xfs *xfsp = XFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;

    do {
	if (!XFS_TOKEN_GOT(xn, tok)) {
	    struct xfs_message_getdata msg;

	    msg.header.opcode = XFS_MSG_GETDATA;
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	    msg.handle = xn->handle;
	    msg.tokens = tok;
	    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	    if (error == 0)
		error = ((struct xfs_message_wakeup *) & msg)->error;
	} else {
	    goto done;
	}
    } while (error == 0);

done:
    return error;
}

static int
do_fsync(struct xfs * xfsp,
	 struct xfs_node * xn,
	 struct ucred * cred,
	 u_int flag)
{
    int error;
    struct xfs_message_putdata msg;

    msg.header.opcode = XFS_MSG_PUTDATA;
    if (cred != NOCRED) {
	msg.cred.uid = cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cred);
    } else {
	msg.cred.uid = 0;
	msg.cred.pag = XFS_ANONYMOUSID;
    }
    msg.handle = xn->handle;
    vattr2xfs_attr(&xn->attr, &msg.attr);
    msg.flag   = flag;

    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
    if (error == 0)
	error = ((struct xfs_message_wakeup *) & msg)->error;

    if (error == 0)
	xn->flags &= ~XFS_DATA_DIRTY;

    return error;
}

int
xfs_fsync_common(struct vnode *vp, struct ucred *cred,
		 int waitfor, struct proc *proc)
{
    struct xfs *xfsp = XFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_fsync: %p\n", vp));

    /*
     * It seems that fsync is sometimes called after reclaiming a node.
     * In that case we just look happy.
     */

    if (xn == NULL) {
	printf("XFS PANIC WARNING! xfs_fsync called after reclaiming!\n");
	return 0;
    }
    
    if (xn->flags & XFS_DATA_DIRTY)
	error = do_fsync(xfsp, xn, cred, XFS_WRITE);

    return error;
}

int
xfs_close_common(struct vnode *vp, int fflag,
		 struct proc *proc, struct ucred *cred)
{
    struct xfs *xfsp = XFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;
    
    XFSDEB(XDEBVNOPS, ("xfs_close cred = %p\n", cred));

    if (fflag & FWRITE && xn->flags & XFS_DATA_DIRTY)
	error = do_fsync(xfsp, xn, cred, XFS_WRITE);

    return error;
}

int
xfs_read_common(struct vnode *vp, struct uio *uio, int ioflag, struct ucred *cred)
{
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_read\n"));

    error = xfs_data_valid(vp, cred, XFS_DATA_R);

    if (error == 0) {
	struct vnode *t = DATA_FROM_VNODE(vp);

	xfs_vfs_readlock(t, xfs_uio_to_proc(uio));
#ifdef __osf__
	VOP_READ(t, uio, ioflag, cred, error);
#else /* __osf__ */
	error = VOP_READ(t, uio, ioflag, cred);
#endif /* __osf__ */
	xfs_vfs_unlock(t, xfs_uio_to_proc(uio));
    }
    return error;
}

int
xfs_write_common(struct vnode *vp, struct uio *uiop, int ioflag, struct ucred *cred)
{
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_write\n"));

    error = xfs_data_valid(vp, cred, XFS_DATA_W);

    if (error == 0) {
	struct xfs_node *xn = VNODE_TO_XNODE(vp);
	struct vnode *t = DATA_FROM_XNODE(xn);
	struct vattr sub_attr;
	int error2 = 0;
 
	xfs_vfs_writelock(t, xfs_uio_to_proc(uiop));
#ifdef __osf__
	VOP_WRITE(t, uiop, ioflag, cred, error);
	VNODE_TO_XNODE(vp)->flags |= XFS_DATA_DIRTY;
	VOP_GETATTR(t, &sub_attr, cred, error2);
#else /* __osf__ */
	error = VOP_WRITE(t, uiop, ioflag, cred);
	VNODE_TO_XNODE(vp)->flags |= XFS_DATA_DIRTY;
	error2 = VOP_GETATTR(t, &sub_attr, cred, uiop->uio_procp);
#endif /* __osf__ */

	if (error2 == 0) {
	    xn->attr.va_size  = sub_attr.va_size;
	    xn->attr.va_mtime = sub_attr.va_mtime;
#ifdef UVM
	    uvm_vnp_setsize(vp, sub_attr.va_size);
#else
#ifdef HAVE_KERNEL_VNODE_PAGER_SETSIZE
	    vnode_pager_setsize(vp, sub_attr.va_size);
#endif
#endif
	}
	xfs_vfs_unlock(t, xfs_uio_to_proc(uiop));
    }

    return error;
}

int
xfs_getattr_common(struct vnode *vp, struct vattr *vap, struct ucred *cred)
{
    int error = 0;

    struct xfs_node *xn = VNODE_TO_XNODE(vp);

    XFSDEB(XDEBVNOPS, ("xfs_getattr\n"));

    error = xfs_attr_valid(vp, cred, XFS_ATTR_R);
    if (error == 0)
	*vap = xn->attr;
    return error;
}

int
xfs_setattr_common(struct vnode *vp, struct vattr *vap, struct ucred *cred)
{
    struct xfs *xfsp = XFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_setattr\n"));

#define CHECK_XFSATTR(A, cast) (vap->A == cast VNOVAL || vap->A == xn->attr.A)
	if (CHECK_XFSATTR(va_mode,(mode_t)) &&
	    CHECK_XFSATTR(va_nlink,) &&
	    CHECK_XFSATTR(va_size,) &&
	    CHECK_XFSATTR(va_uid,) &&
	    CHECK_XFSATTR(va_gid,) &&
	    CHECK_XFSATTR(va_mtime.tv_sec,) &&
	    CHECK_XFSATTR(va_fileid,) &&
	    CHECK_XFSATTR(va_type,))
		return 0;		/* Nothing to do */
#undef CHECK_XFSATTR

    if (XFS_TOKEN_GOT(xn, XFS_ATTR_W)) {
	/* Update attributes and mark them dirty. */
	VNODE_TO_XNODE(vp)->flags |= XFS_ATTR_DIRTY;
	error = EINVAL;		       /* XXX not yet implemented */
	goto done;
    } else {
	struct xfs_message_putattr msg;

	msg.header.opcode = XFS_MSG_PUTATTR;
	if (cred != NOCRED) {
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	} else {
	    msg.cred.uid = 0;
	    msg.cred.pag = XFS_ANONYMOUSID;
	}
	msg.handle = xn->handle;
	vattr2xfs_attr(vap, &msg.attr);

	XFS_TOKEN_CLEAR(xn, XFS_ATTR_VALID, XFS_ATTR_MASK);
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) & msg)->error;
    }

done:
    return error;
}

static int
check_rights (u_char rights, int mode)
{
    int error = 0;

    if (mode & VREAD)
	if ((rights & XFS_RIGHT_R) == 0)
	    error = EACCES;
    if (mode & VWRITE)
	if ((rights & XFS_RIGHT_W) == 0)
	    error = EACCES;
    if (mode & VEXEC)
	if ((rights & XFS_RIGHT_X) == 0)
	    error = EACCES;
    return error;
}

int
xfs_access_common(struct vnode *vp, int mode, struct ucred *cred)
{
    int error = 0;
    pag_t pag = xfs_get_pag(cred);

    XFSDEB(XDEBVNOPS, ("xfs_access mode = 0%o\n", mode));

    error = xfs_attr_valid(vp, cred, XFS_ATTR_R);
    if (error == 0) {
	struct xfs_node *xn = VNODE_TO_XNODE(vp);
	int i;

	error = check_rights (xn->anonrights, mode);

	if (error == 0)
	    goto done;

	XFSDEB(XDEBVNOPS, ("xfs_access anonaccess failed\n"));

	xfs_fetch_rights(vp, cred); /* ignore error */

	error = EACCES;		/* default to EACCES if pag isn't in xn->id */

	for (i = 0; i < MAXRIGHTS; i++)
	    if (xn->id[i] == pag) {
		error = check_rights (xn->rights[i], mode);
		break;
	    }
    }

done:
    XFSDEB(XDEBVNOPS, ("xfs_access(0%o) = %d\n", mode, error));

    return error;
}

int
xfs_lookup_common(struct vnode *dvp, 
		  xfs_componentname *cnp, 
		  struct vnode **vpp)
{
    struct xfs_message_getnode msg;
    struct xfs *xfsp = XFS_FROM_VNODE(dvp);
    struct xfs_node *d = VNODE_TO_XNODE(dvp);
    int error = 0;
    struct proc *proc  = xfs_cnp_to_proc(cnp);
    struct ucred *cred = xfs_proc_to_cred(proc);

    *vpp = NULL;

    if (dvp->v_type != VDIR)
	return ENOTDIR;
    
 again:

    do {
#ifdef __osf__
	VOP_ACCESS(dvp, VEXEC, cred, error);
#else
	error = VOP_ACCESS(dvp, VEXEC, cred, xfs_cnp_to_proc(cnp));
#endif
	if (error != 0)
	    goto done;

	error = xfs_dnlc_lookup(dvp, cnp, vpp);
	if (error == 0) {

	    /*
	     * Doesn't quite work.
	     */

#if 0
	    if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME)
		&& (cnp->cn_flags & ISLASTCN)) {
		error = EJUSTRETURN;
		goto done;
	    }
#endif

	    msg.header.opcode = XFS_MSG_GETNODE;
	    if (cnp->cn_cred != NOCRED) {
		msg.cred.uid = cnp->cn_cred->cr_uid;
		msg.cred.pag = xfs_get_pag(cnp->cn_cred);
	    } else {
		msg.cred.uid = 0;
		msg.cred.pag = XFS_ANONYMOUSID;
	    }
	    msg.parent_handle = d->handle;

	    bcopy(cnp->cn_nameptr, msg.name, cnp->cn_namelen);
	    msg.name[cnp->cn_namelen] = '\0';

	    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));

	    if (error == 0)
		error = ((struct xfs_message_wakeup *) & msg)->error;
	    if(error == ENOENT && cnp->cn_nameiop != CREATE) {
		XFSDEB(XDEBVNOPS, ("xfs_lookup: neg cache %p (%s, %ld)\n",
				   dvp,
				   cnp->cn_nameptr, cnp->cn_namelen));
		xfs_dnlc_enter (dvp, cnp, NULL);
	    }
	} else if (error == -1) {
	    if (xfs_do_vget(*vpp, LK_EXCLUSIVE, xfs_cnp_to_proc(cnp)))
		    goto again;
	    error = 0;
	    goto done;
	}
    } while (error == 0);

done:
    return error;
}

int
xfs_lookup_name(struct vnode *dvp, 
		const char *name,
		struct proc *proc,
		struct ucred *cred,
		struct vnode **vpp)
{
    xfs_componentname cn;

    xfs_cnp_init (&cn, name, NULL, dvp, proc, cred, CREATE);
    return xfs_lookup_common (dvp, &cn, vpp);
}    

int
xfs_create_common(struct vnode *dvp,
		  const char *name,
		  struct vattr *vap, 
		  struct ucred *cred)
{
    struct xfs *xfsp = XFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_create: (%s)\n", name));
    {
	struct xfs_message_create msg;

	msg.header.opcode = XFS_MSG_CREATE;
	msg.parent_handle = xn->handle;
	strncpy(msg.name, name, 256);
	vattr2xfs_attr(vap, &msg.attr);

	msg.mode = 0;		       /* XXX - mode */
	if (cred != NOCRED) {
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	} else {
	    msg.cred.uid = 0;
	    msg.cred.pag = XFS_ANONYMOUSID;
	}

	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) & msg)->error;
    }

#if 0
    if (error == EEXIST)
	error = 0;
#endif

    return error;
}

int
xfs_remove_common(struct vnode *dvp,
		  struct vnode *vp,
		  const char *name,
		  struct ucred *cred)
{
    struct xfs *xfsp  = XFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    struct xfs_message_remove msg;
    int error;

    XFSDEB(XDEBVNOPS, ("xfs_remove: %s\n", name));

    msg.header.opcode = XFS_MSG_REMOVE;
    msg.parent_handle = xn->handle;
    strncpy(msg.name, name, 256);
    msg.cred.uid = cred->cr_uid;
    msg.cred.pag = xfs_get_pag(cred);
    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
    if (error == 0)
	error = ((struct xfs_message_wakeup *) &msg)->error;

    if (error == 0)
	xfs_dnlc_purge (vp);

#if !defined(__FreeBSD__) || __FreeBSD_version < 300000
    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);
    vput(dvp);
#endif
    
    return error;
}

int
xfs_rename_common(struct vnode *fdvp, 
		  struct vnode *fvp,
		  const char *fname,
		  struct vnode *tdvp,
		  struct vnode *tvp,
		  const char *tname,
		  struct ucred *cred)
{
    struct xfs *xfsp = XFS_FROM_VNODE(fdvp);
    int error;

    XFSDEB(XDEBVNOPS, ("xfs_rename: %s %s\n", fname, tname));

    if ((fvp->v_mount != tdvp->v_mount)
	|| (tvp && (fvp->v_mount != tvp->v_mount))) {
	return  EXDEV;
    }

    {
	struct xfs_message_rename msg;

	msg.header.opcode = XFS_MSG_RENAME;
	msg.old_parent_handle = VNODE_TO_XNODE(fdvp)->handle;
	strncpy(msg.old_name, fname, 256);
	msg.new_parent_handle = VNODE_TO_XNODE(tdvp)->handle;
	strncpy(msg.new_name, tname, 256);
	msg.cred.uid = cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cred);
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) &msg)->error;

    }

    XFSDEB(XDEBVNOPS, ("xfs_rename: error = %d\n", error));

    return error;
}

int
xfs_mkdir_common(struct vnode *dvp, 
		 const char *name,
		 struct vattr *vap, 
		 struct ucred *cred)
{
    struct xfs *xfsp = XFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_mkdir: %s\n", name));
    {
	struct xfs_message_mkdir msg;

	msg.header.opcode = XFS_MSG_MKDIR;
	msg.parent_handle = xn->handle;
	strncpy(msg.name, name, 256);
	vattr2xfs_attr(vap, &msg.attr);
	if (cred != NOCRED) {
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	} else {
	    msg.cred.uid = 0;
	    msg.cred.pag = XFS_ANONYMOUSID;
	}
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) & msg)->error;
    }

    return error;
}

int
xfs_rmdir_common(struct vnode *dvp,
		 struct vnode *vp,
		 const char *name,
		 struct ucred *cred)
{
    struct xfs *xfsp  = XFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    struct xfs_message_rmdir msg;
    int error;

    XFSDEB(XDEBVNOPS, ("xfs_rmdir: %s\n", name));

    msg.header.opcode = XFS_MSG_RMDIR;
    msg.parent_handle = xn->handle;
    strncpy(msg.name, name, 256);
    msg.cred.uid = cred->cr_uid;
    msg.cred.pag = xfs_get_pag(cred);
    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
    if (error == 0)
	error = ((struct xfs_message_wakeup *) &msg)->error;

    if (error == 0)
	xfs_dnlc_purge (vp);

#if !defined(__FreeBSD__) || __FreeBSD_version < 300000
    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);
    vput(dvp);
#endif

    return error;
}

int
xfs_readdir_common(struct vnode *vp, 
		   struct uio *uiop, 
		   struct ucred *cred,
		   int *eofflag)
{
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_readdir\n"));

    if(eofflag)
	*eofflag = 0;
    error = xfs_data_valid(vp, cred, XFS_DATA_R);
    if (error == 0) {
	struct vnode *t = DATA_FROM_VNODE(vp);

	xfs_vfs_readlock(t, xfs_uio_to_proc(uiop));
#ifdef __osf__
	VOP_READ(t, uiop, 0, cred, error);
#else
	error = VOP_READ(t, uiop, 0, cred);
#endif
	if(eofflag)
	    *eofflag = VNODE_TO_XNODE(vp)->attr.va_size <= uiop->uio_offset;
	xfs_vfs_unlock(t, xfs_uio_to_proc(uiop));
    }
    return error;
}

int
xfs_link_common(struct vnode *dvp, 
		struct vnode *vp, 
		const char *name,
		struct ucred *cred)
{
    struct xfs *xfsp = XFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    struct xfs_node *xn2 = VNODE_TO_XNODE(vp);
    struct xfs_message_link msg;
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_link: %s\n", name));
    
    msg.header.opcode = XFS_MSG_LINK;
    msg.parent_handle = xn->handle;
    msg.from_handle   = xn2->handle;
    strncpy(msg.name, name, 256);
    msg.cred.uid = cred->cr_uid;
    msg.cred.pag = xfs_get_pag(cred);

    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
    if (error == 0)
	error = ((struct xfs_message_wakeup *) & msg)->error;
    
    return error;
}

int
xfs_symlink_common(struct vnode *dvp,
		   struct vnode **vpp,
		   const char *name,
		   struct proc *proc,
		   struct ucred *cred,
		   struct vattr *vap,
		   char *target)
{
    struct xfs *xfsp = XFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    struct xfs_message_symlink msg;
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_symlink: %s\n", name));

    msg.header.opcode = XFS_MSG_SYMLINK;
    msg.parent_handle = xn->handle;
    strncpy(msg.name, name, sizeof(msg.name));
    msg.name[sizeof(msg.name) - 1] = '\0';
    vattr2xfs_attr(vap, &msg.attr);
    msg.cred.uid = cred->cr_uid;
    msg.cred.pag = xfs_get_pag(cred);
    strncpy (msg.contents, target, sizeof(msg.contents));
    msg.contents[sizeof(msg.contents) - 1] = '\0';

    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
    if (error == 0)
	error = ((struct xfs_message_wakeup *) & msg)->error;

    if (error == 0) {
	error = xfs_lookup_name(dvp, name, proc, cred, vpp);
	if (error == 0)
	    vput (*vpp);
    }

#if !defined(__FreeBSD__) || __FreeBSD_version < 300000
    vput(dvp);
#endif

    return error;
}

int
xfs_readlink_common(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_readlink\n"));

    error = xfs_data_valid(vp, cred, XFS_DATA_R);
    if (error == 0) {
	struct vnode *t = DATA_FROM_VNODE(vp);

	xfs_vfs_readlock(t, xfs_uio_to_proc(uiop));
#ifdef __osf__
	VOP_READ(t, uiop, 0, cred, error);
#else
	error = VOP_READ(t, uiop, 0, cred);
#endif
	xfs_vfs_unlock(t, xfs_uio_to_proc(uiop));
    }
    return error;
}

int
xfs_inactive_common(struct vnode *vp, struct proc *p)
{
    struct xfs_message_inactivenode msg;
    struct xfs *xfsp = XFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);

    XFSDEB(XDEBVNOPS, ("xfs_inactive, %p\n", vp));

    /*
     * This seems rather bogus, but sometimes we get an already
     * cleaned node to be made inactive.  Just ignoring it seems safe.
     */

    if (xn == NULL) {
	XFSDEB(XDEBVNOPS, ("xfs_inactive: clean node\n"));
	return 0;
    }

    xn->tokens = 0;
    msg.header.opcode = XFS_MSG_INACTIVENODE;
    msg.handle = xn->handle;
    msg.flag   = XFS_NOREFS;
    xfs_message_send(xfsp->fd, &msg.header, sizeof(msg));

#ifndef __osf__
    xfs_vfs_unlock(vp, p);
#else
    /* XXX ? */
#endif

    XFSDEB(XDEBVNOPS, ("return: xfs_inactive\n"));

    return 0;
}

int
xfs_reclaim_common(struct vnode *vp)
{
    struct xfs_message_inactivenode msg;
    struct xfs *xfsp = XFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);

    XFSDEB(XDEBVNOPS, ("xfs_reclaim: %p\n", vp));

    msg.header.opcode = XFS_MSG_INACTIVENODE;
    msg.handle = xn->handle;
    msg.flag   = XFS_NOREFS | XFS_DELETE;
    xfs_message_send(xfsp->fd, &msg.header, sizeof(msg));

    xfs_dnlc_purge(vp);
    free_xfs_node(xn);

    return 0;
}

/*
 *
 */

#if 0

int
xfs_advlock_common(struct vnode *dvp, 
		   int locktype,
		   unsigned long lockid, /* XXX this good ? */
		   struct ucred *cred)
{
    struct xfs *xfsp = XFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    int error = 0;

    XFSDEB(XDEBVNOPS, ("xfs_advlock\n"));
    {
	struct xfs_message_advlock msg;

	msg.header.opcode = XFS_MSG_ADVLOCK;
	msg.handle = xn->handle;
	msg.locktype = locktype;
	msg.lockid = lockid;

	if (cred != NOCRED) {
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	} else {
	    msg.cred.uid = 0;
	    msg.cred.pag = XFS_ANONYMOUSID;
	}
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) & msg)->error;
    }

    if (error == 0) {
	
	/* sleep until woken */

    } else {

	/* die */
    }

    return error;
}

#endif

/*
 *
 */

void
xfs_printnode_common (struct vnode *vp)
{
    struct xfs_node *xn = VNODE_TO_XNODE(vp);

    printf ("xnode: fid: %d.%d.%d.%d\n", 
	    xn->handle.a, xn->handle.b, xn->handle.c, xn->handle.d);
    printf ("\tattr: %svalid\n", 
	    XFS_TOKEN_GOT(xn, XFS_ATTR_VALID) ? "": "in");
    printf ("\tdata: %svalid\n", 
	    XFS_TOKEN_GOT(xn, XFS_DATA_VALID) ? "": "in");
    printf ("\tflags: 0x%x\n", xn->flags);
}
