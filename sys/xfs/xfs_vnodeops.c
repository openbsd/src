/*	$OpenBSD: xfs_vnodeops.c,v 1.2 1998/08/31 05:13:20 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/fcntl.h>

#include <xfs/xfs_message.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_common.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_deb.h>
#include <xfs/xfs_syscalls.h>

RCSID("$KTH: xfs_vnodeops.c,v 1.41 1998/08/14 04:54:09 art Exp $");

static int
xfs_open_valid(struct vnode *vp, struct ucred *cred, u_int tok)
{
	struct xfs	*xfsp = XFS_FROM_VNODE(vp);
	struct xfs_node	*xn = VNODE_TO_XNODE(vp);
	int		error = 0;

	XFSDEB(XDEBVFOPS, ("xfs_open_valid\n"));

	do {
		if (!XFS_TOKEN_GOT(xn, tok)) {
			struct xfs_message_open msg;

			msg.header.opcode = XFS_MSG_OPEN;
			msg.cred.uid = cred->cr_uid;
			msg.cred.pag = xfs_get_pag(cred);
			msg.handle = xn->handle;
			msg.tokens = tok;
			error = xfs_message_rpc(xfsp->fd, &msg.header,
						sizeof(msg));
			if (error == 0)
				error = ((struct xfs_message_wakeup *)&msg)->error;
		} else {
			goto done;
		}
	} while (error == 0);

 done:
	return error;
}

static int
xfs_attr_valid(struct vnode *vp, struct ucred *cred, u_int tok)
{
	struct xfs	*xfsp = XFS_FROM_VNODE(vp);
	struct xfs_node *xn = VNODE_TO_XNODE(vp);
	int		error = 0;
	pag_t		pag = xfs_get_pag(cred);

	do {
		if (!XFS_TOKEN_GOT(xn, tok)) {
			struct xfs_message_getattr msg;

			msg.header.opcode = XFS_MSG_GETATTR;
			msg.cred.uid = cred->cr_uid;
			msg.cred.pag = pag;
			msg.handle = xn->handle;
			error = xfs_message_rpc(xfsp->fd, &msg.header,
						sizeof(msg));
			if (error == 0)
				error =	((struct xfs_message_wakeup *) &msg)->error;
		} else {
			goto done;
		}
	} while (error == 0);

 done:
	return error;
}

static int
xfs_rights_valid(struct vnode *vp, struct ucred *cred)
{
	struct xfs	*xfsp = XFS_FROM_VNODE(vp);
	struct xfs_node	*xn = VNODE_TO_XNODE(vp);
	int		error = 0;
	pag_t		pag = xfs_get_pag(cred);

	do {
		if (!xfs_has_pag(xn, pag)) {
			struct xfs_message_getattr msg;

			msg.header.opcode = XFS_MSG_GETATTR;
			msg.cred.uid = cred->cr_uid;
			msg.cred.pag = pag;
			msg.handle = xn->handle;
			error = xfs_message_rpc(xfsp->fd, &msg.header,
						sizeof(msg));
			if (error == 0)
				error = ((struct xfs_message_wakeup *) &msg)->error;
		} else {
			goto done;
		}
	} while (error == 0);

 done:
	return error;
}

static int
xfs_data_valid(struct vnode *vp, struct ucred *cred, u_int tok)
{
	struct xfs	*xfsp = XFS_FROM_VNODE(vp);
	struct xfs_node	*xn = VNODE_TO_XNODE(vp);
	int		error = 0;

	do {
		if (!XFS_TOKEN_GOT(xn, tok)) {
			struct xfs_message_getdata msg;

			msg.header.opcode = XFS_MSG_GETDATA;
			msg.cred.uid = cred->cr_uid;
			msg.cred.pag = xfs_get_pag(cred);
			msg.handle = xn->handle;
			msg.tokens = tok;
			error = xfs_message_rpc(xfsp->fd, &msg.header,
						sizeof(msg));
			if (error == 0)
				error = ((struct xfs_message_wakeup *) &msg)->error;
		} else {
			goto done;
		}
	} while (error == 0);

 done:
	return error;
}

static int
do_fsync(struct xfs *xfsp, struct xfs_node *xn, struct ucred *cred, u_int flag)
{
	int		error;
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

	msg.flag = flag;
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
		error = ((struct xfs_message_wakeup *) &msg)->error;

	if (error == 0)
		xn->flags &= ~XFS_DATA_DIRTY;

	return error;
}

/*
 * vnode functions
 */

static int
xfs_open(void *vap)
{
	struct vop_open_args	*ap = vap;

	XFSDEB(XDEBVNOPS, ("xfs_open\n"));

	if (ap->a_mode & FWRITE)
		return xfs_open_valid(ap->a_vp, ap->a_cred, XFS_OPEN_NW);
	else
		return xfs_open_valid(ap->a_vp, ap->a_cred, XFS_OPEN_NR);
}

static int
xfs_fsync(void *vap)
{
	struct vop_fsync_args	*ap = vap;

	struct xfs		*xfsp = XFS_FROM_VNODE(ap->a_vp);
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_vp);
	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_fsync: 0x%x\n", (int) ap->a_vp));

	/*
	 * It seems that fsync is sometimes called after reclaiming a node.
	 * In that case we just look happy.
	 */
	if (xn == NULL) {
		printf("XFS PANIC WARNING! xfs_fsync after reclaiming!\n");
		return 0;
	}

	if (xn->flags & XFS_DATA_DIRTY)
		error = do_fsync(xfsp, xn, ap->a_cred, XFS_WRITE);

	return error;
}

static int
xfs_close(void *vap)
{
	struct vop_close_args	*ap = vap;

	struct xfs		*xfsp = XFS_FROM_VNODE(ap->a_vp);
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_vp);
	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_close cred = %p\n", ap->a_cred));

	if (ap->a_fflag & FWRITE && xn->flags & XFS_DATA_DIRTY)
		error = do_fsync(xfsp, xn, ap->a_cred, XFS_WRITE);

	return error;
}

static int
xfs_read(void *vap)
{
	struct vop_read_args	*ap = vap;

	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_read\n"));

	error = xfs_data_valid(ap->a_vp, ap->a_cred, XFS_DATA_R);
	if (error == 0) {
		struct vnode *t = DATA_FROM_VNODE(ap->a_vp);

		vn_lock(t, LK_EXCLUSIVE | LK_RETRY, ap->a_uio->uio_procp);
		error = VOP_READ(t, ap->a_uio, ap->a_ioflag, ap->a_cred);
		VOP_UNLOCK(t, 0, ap->a_uio->uio_procp);
	}

	return error;
}

static int
xfs_write(void *vap)
{
	struct vop_read_args	*ap = vap;

	struct vnode		*vp = ap->a_vp;
	struct uio		*uiop = ap->a_uio;
	int			ioflag = ap->a_ioflag;
	struct ucred		*cred = ap->a_cred;
	int 			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_write\n"));

	error = xfs_data_valid(vp, cred, XFS_DATA_W);
	if (error == 0) {
		struct xfs_node	*xn = VNODE_TO_XNODE(vp);    
		struct vnode	*t = DATA_FROM_XNODE(xn);
		struct vattr	sub_attr;
		int		error2 = 0;

		vn_lock(t, LK_EXCLUSIVE | LK_RETRY, uiop->uio_procp);
		error = VOP_WRITE(t, uiop, ioflag, cred);
		VNODE_TO_XNODE(ap->a_vp)->flags |= XFS_DATA_DIRTY;

		/* Update size an mtime on the xfs node from the cache node */
		error2 = VOP_GETATTR(t, &sub_attr, cred, uiop->uio_procp);
		if (error2 == 0) {
			xn->attr.va_size  = sub_attr.va_size;
			xn->attr.va_mtime = sub_attr.va_mtime;
		}

		VOP_UNLOCK(t, 0, ap->a_uio->uio_procp);
	}

	return error;
}

static int
xfs_getattr(void *vap)
{
	struct vop_getattr_args	*ap = vap;

	int			error = 0;
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_vp);

	XFSDEB(XDEBVNOPS, ("xfs_getattr\n"));

	error = xfs_attr_valid(ap->a_vp, ap->a_cred, XFS_ATTR_R);
	if (error == 0) {
		*ap->a_vap = xn->attr;
	}

	return error;
}

static int
xfs_setattr(void *vap)
{
	struct vop_setattr_args	*ap = vap;

	struct xfs		*xfsp = XFS_FROM_VNODE(ap->a_vp);
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_vp);
	int	error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_setattr\n"));
	if (XFS_TOKEN_GOT(xn, XFS_ATTR_W)) {
		/* Update attributes and mark them dirty. */
		VNODE_TO_XNODE(ap->a_vp)->flags |= XFS_ATTR_DIRTY;
		error = EINVAL;		       /* XXX not yet implemented */
		goto done;
	} else {
		struct xfs_message_putattr msg;

		msg.header.opcode = XFS_MSG_PUTATTR;
		if (ap->a_cred != NOCRED) {
			msg.cred.uid = ap->a_cred->cr_uid;
			msg.cred.pag = xfs_get_pag(ap->a_cred);
		} else {
			msg.cred.uid = 0;
			msg.cred.pag = XFS_ANONYMOUSID;
		}
		msg.handle = xn->handle;
		vattr2xfs_attr(ap->a_vap, &msg.attr);

		XFS_TOKEN_CLEAR(xn, XFS_ATTR_VALID, XFS_ATTR_MASK);
		error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
		if (error == 0)
			error = ((struct xfs_message_wakeup *) &msg)->error;
	}

 done:
	return error;
}

/*
 * HANDS OFF! Don't touch this one if you aren't really really sure what you
 * are doing!
 */
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

/*
 * HANDS OFF! Don't touch this one if you aren't really really sure what you
 * are doing! 
 */
static int
xfs_access(void *vap)
{
	struct vop_access_args *ap = vap;

	int		error = 0;
	int		mode = ap->a_mode;
	pag_t		pag = xfs_get_pag(ap->a_cred);

	XFSDEB(XDEBVNOPS, ("xfs_access mode = 0%o\n", mode));

	error = xfs_attr_valid(ap->a_vp, ap->a_cred, XFS_ATTR_R);
	if (error == 0) {
		struct xfs_node	*xn = VNODE_TO_XNODE(ap->a_vp);
		int		i;

		error = check_rights (xn->anonrights, mode);
		if (error == 0)
			goto done;

		XFSDEB(XDEBVNOPS, ("xfs_access anonaccess failed\n"));

		xfs_rights_valid(ap->a_vp, ap->a_cred); /* ignore error */

		/* default to EACCES if pag isn't in xn->id */
		error = EACCES;		

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

/*
 * Do the actual lookup. The locking state of dvp is not changed and vpp is
 * returned locked and ref:d.
 *
 * This assumes that the cache doesn't change the locking state,
 * which it shouldn't.
 */
static int
do_actual_lookup(struct vnode *dvp, struct componentname *cnp,
		 struct vnode **vpp)
{
	int			error;
	struct xfs_node		*d = VNODE_TO_XNODE(dvp);
	struct xfs		*xfsp = XFS_FROM_VNODE(dvp);
	struct xfs_message_getnode msg;

	if (dvp->v_type != VDIR)
		return ENOTDIR;

	do {
		error = xfs_dnlc_lookup(dvp, cnp, vpp);
		if (error == 0) {
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

			error = xfs_message_rpc(xfsp->fd, &msg.header,
						sizeof(msg));
			if (error == 0)
				error = ((struct xfs_message_wakeup *) &msg)->error;
			if (error == ENOENT && cnp->cn_namelen <= NCHNAMLEN) {
				XFSDEB(XDEBVNOPS,
				       ("xfs_lookup: neg cache %p (%s, %ld)\n",
					dvp,
					cnp->cn_nameptr, cnp->cn_namelen));
				cache_enter (dvp, NULL, cnp);
			}

			XFSDEB(XDEBVNOPS, ("xfs_lookup error: %d\n", error));
		} else if (error == -1) {
			vget(*vpp, 0, cnp->cn_proc);
			error = 0;
			goto done;
		}
	} while (error == 0);

 done:
	return error;
}

static int
xfs_lookup(void *vap)
{
	struct vop_lookup_args	*ap = vap;

	struct vnode		*dvp = ap->a_dvp;
	struct componentname	*cnp = ap->a_cnp;
	int			nameiop = cnp->cn_nameiop;
	int			flags = cnp->cn_flags;
	int			islastcn = flags & ISLASTCN;
	struct proc		*p = cnp->cn_proc;
	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_lookup: (%s, %ld)\n",
			   cnp->cn_nameptr,
			   cnp->cn_namelen));

	*ap->a_vpp = NULL;

	error = do_actual_lookup(dvp, cnp, ap->a_vpp);
	if (error == ENOENT
	    && (nameiop == CREATE || nameiop == RENAME)
	    && islastcn) {
		error = EJUSTRETURN;
	}

	if (nameiop != LOOKUP && islastcn)
		cnp->cn_flags |= SAVENAME;

	if ((error == EJUSTRETURN || error == 0) &&
	    !(islastcn && flags & LOCKPARENT))
		VOP_UNLOCK(ap->a_dvp, 0, p);

	XFSDEB(XDEBVNOPS, ("xfs_lookup() error = %d\n", error));
	return error;
}

static int
xfs_create(void *vap)
{
	struct vop_create_args	*ap = vap;

	struct xfs		*xfsp = XFS_FROM_VNODE(ap->a_dvp);
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_dvp);
	struct componentname	*cnp = ap->a_cnp;
	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_create: (%s, %ld)\n",
			   cnp->cn_nameptr,
			   cnp->cn_namelen));
	{
		struct xfs_message_create msg;

		msg.header.opcode = XFS_MSG_CREATE;
		msg.parent_handle = xn->handle;
		strncpy(msg.name, cnp->cn_nameptr, 256);
		vattr2xfs_attr(ap->a_vap, &msg.attr);

		msg.mode = 0;		       /* XXX - mode */
		if (cnp->cn_cred != NOCRED) {
			msg.cred.uid = cnp->cn_cred->cr_uid;
			msg.cred.pag = xfs_get_pag(cnp->cn_cred);
		} else {
			msg.cred.uid = 0;
			msg.cred.pag = XFS_ANONYMOUSID;
		}

		error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
		if (error == 0)
			error = ((struct xfs_message_wakeup *) &msg)->error;
    }

	if (error == 0) {
		error = do_actual_lookup(ap->a_dvp, cnp, ap->a_vpp);
	}

	if (error != 0 || (cnp->cn_flags & SAVESTART) == 0)
		free(cnp->cn_pnbuf, M_NAMEI);

	vput(ap->a_dvp);

	return error;
}

static int
xfs_remove(void *vap)
{
	struct vop_remove_args	*ap = vap;
	struct vnode		*dvp = ap->a_dvp;
	struct vnode		*vp = ap->a_vp;
	struct xfs		*xfsp = XFS_FROM_VNODE(dvp);
	struct xfs_node		*xn = VNODE_TO_XNODE(dvp);
	struct componentname	*cnp = ap->a_cnp;
	struct xfs_message_remove msg;
	int			error;

	XFSDEB(XDEBVNOPS, ("xfs_remove: (%s, %ld\n",
			   cnp->cn_nameptr,
			   cnp->cn_namelen));

	msg.header.opcode = XFS_MSG_REMOVE;
	msg.parent_handle = xn->handle;
	strncpy(msg.name, cnp->cn_nameptr, 256);
	msg.cred.uid = cnp->cn_cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cnp->cn_cred);

	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
		error = ((struct xfs_message_wakeup *) &msg)->error;

	if (error == 0)
		cache_purge (vp);

	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);

	vput(dvp);

	if (error != 0 || (cnp->cn_flags & SAVESTART) == 0)
		free(cnp->cn_pnbuf, M_NAMEI);

	return error;
}

static int
xfs_rename(void *vap)
{
	struct vop_rename_args	*ap = vap;

	struct vnode		*fdvp = ap->a_fdvp;
	struct vnode		*fvp  = ap->a_fvp;
	struct componentname	*fcnp = ap->a_fcnp;
	struct vnode		*tdvp = ap->a_tdvp;
	struct vnode		*tvp  = ap->a_tvp;
	struct componentname	*tcnp = ap->a_tcnp;
	struct xfs		*xfsp = XFS_FROM_VNODE(fdvp);
	int			error;

	XFSDEB(XDEBVNOPS, ("xfs_rename\n"));

	if ((fvp->v_mount != tdvp->v_mount)
	    || (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto abort;
	}

	if (tvp) {
		struct xfs_message_remove msg;

		msg.header.opcode = XFS_MSG_REMOVE;
		msg.parent_handle = VNODE_TO_XNODE(tdvp)->handle;
		strncpy(msg.name, tcnp->cn_nameptr, 256);
		msg.cred.uid = tcnp->cn_cred->cr_uid;
		msg.cred.pag = xfs_get_pag(tcnp->cn_cred);

		error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
		if (error == 0)
			error = ((struct xfs_message_wakeup *) &msg)->error;

		if (error)
			goto abort;

		vput(tvp);
		tvp = NULL;
	}

	{
		struct xfs_message_rename msg;

		msg.header.opcode = XFS_MSG_RENAME;
		msg.old_parent_handle = VNODE_TO_XNODE(fdvp)->handle;
		strncpy(msg.old_name, fcnp->cn_nameptr, 256);
		msg.new_parent_handle = VNODE_TO_XNODE(tdvp)->handle;
		strncpy(msg.new_name, tcnp->cn_nameptr, 256);
		msg.cred.uid = tcnp->cn_cred->cr_uid;
		msg.cred.pag = xfs_get_pag(tcnp->cn_cred);

		error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
		if (error == 0)
			error = ((struct xfs_message_wakeup *) &msg)->error;
	}

 abort:
	VOP_ABORTOP(tdvp, tcnp);
	if(tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if(tvp)
		vput(tvp);
	VOP_ABORTOP(fdvp, fcnp);
	vrele(fdvp);
	vrele(fvp);

	return error;
}

static int
xfs_mkdir(void *vap)
{
	struct vop_mkdir_args	*ap = vap;

	struct xfs		*xfsp = XFS_FROM_VNODE(ap->a_dvp);
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_dvp);
	struct componentname	*cnp = ap->a_cnp;
	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_mkdir\n"));
	{
		struct xfs_message_mkdir msg;

		msg.header.opcode = XFS_MSG_MKDIR;
		msg.parent_handle = xn->handle;
		strncpy(msg.name, cnp->cn_nameptr, 256);
		vattr2xfs_attr(ap->a_vap, &msg.attr);
		if (cnp->cn_cred != NOCRED) {
			msg.cred.uid = cnp->cn_cred->cr_uid;
			msg.cred.pag = xfs_get_pag(cnp->cn_cred);
		} else {
			msg.cred.uid = 0;
			msg.cred.pag = XFS_ANONYMOUSID;
		}

		error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
		if (error == 0)
			error = ((struct xfs_message_wakeup *) &msg)->error;
	}

	if (error == 0) {
		error = do_actual_lookup(ap->a_dvp, cnp, ap->a_vpp);
	}

	if (error != 0 || (cnp->cn_flags & SAVESTART) == 0)
		free(cnp->cn_pnbuf, M_NAMEI);

	vput(ap->a_dvp);
	return error;
}

static int
xfs_rmdir(void *vap)
{
	struct vop_rmdir_args	*ap = vap;

	struct vnode		*dvp = ap->a_dvp;
	struct vnode		*vp = ap->a_vp;
	struct xfs		*xfsp = XFS_FROM_VNODE(dvp);
	struct xfs_node		*xn = VNODE_TO_XNODE(dvp);
	struct componentname	*cnp = ap->a_cnp;
	struct xfs_message_rmdir msg;
	int			error;

	XFSDEB(XDEBVNOPS, ("xfs_rmdir: (%s, %ld\n",
			   cnp->cn_nameptr,
			   cnp->cn_namelen));

	msg.header.opcode = XFS_MSG_RMDIR;
	msg.parent_handle = xn->handle;
	strncpy(msg.name, cnp->cn_nameptr, 256);
	msg.cred.uid = cnp->cn_cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cnp->cn_cred);

	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
		error = ((struct xfs_message_wakeup *) &msg)->error;

	if (error == 0)
		cache_purge(vp);

	/*
	 * sys_rmdir should not allow this to happen.
	 */
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);

	if (error != 0 || (cnp->cn_flags & SAVESTART) == 0)
		free(cnp->cn_pnbuf, M_NAMEI);

	return error;
}

static int
xfs_readdir(void *vap)
{
	struct vop_readdir_args	*ap = vap;

	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_readdir\n"));

	error = xfs_data_valid(ap->a_vp, ap->a_cred, XFS_DATA_R);
	if (error == 0) {
		struct vnode *t = DATA_FROM_VNODE(ap->a_vp);

		vn_lock(t, LK_EXCLUSIVE | LK_RETRY, ap->a_uio->uio_procp);
		error = VOP_READ(t, ap->a_uio, 0, ap->a_cred);
		VOP_UNLOCK(t, 0, ap->a_uio->uio_procp);
	}

	return error;
}

static int
xfs_link(void *vap)
{
	struct vop_link_args	*ap = vap;

	struct xfs		*xfsp = XFS_FROM_VNODE(ap->a_dvp);
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_dvp);
	struct xfs_node		*xn2 = VNODE_TO_XNODE(ap->a_vp);
	struct componentname	*cnp = ap->a_cnp;
	struct xfs_message_link	msg;
	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_link: (%s, %ld\n",
			   cnp->cn_nameptr,
			   cnp->cn_namelen));

	msg.header.opcode = XFS_MSG_LINK;
	msg.parent_handle = xn->handle;
	msg.from_handle   = xn2->handle;
	strncpy(msg.name, cnp->cn_nameptr, 256);
	msg.cred.uid = cnp->cn_cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cnp->cn_cred);

	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
		error = ((struct xfs_message_wakeup *) &msg)->error;

	if (error != 0 || (cnp->cn_flags & SAVESTART) == 0)
		free(cnp->cn_pnbuf, M_NAMEI);

	vput(ap->a_dvp);
	return error;
}

static int
xfs_symlink(void *vap)
{
	struct vop_symlink_args	*ap = vap;

	struct xfs		*xfsp = XFS_FROM_VNODE(ap->a_dvp);
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_dvp);
	struct componentname	*cnp = ap->a_cnp;
	struct xfs_message_symlink msg;
	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_symlink: (%s, %ld)\n",
			   cnp->cn_nameptr,
			   cnp->cn_namelen));

	msg.header.opcode = XFS_MSG_SYMLINK;
	msg.parent_handle = xn->handle;
	strncpy(msg.name, cnp->cn_nameptr, 256);
	vattr2xfs_attr(ap->a_vap, &msg.attr);
	msg.cred.uid = cnp->cn_cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cnp->cn_cred);
	strncpy (msg.contents, ap->a_target, sizeof(msg.contents));

	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
		error = ((struct xfs_message_wakeup *) &msg)->error;

	if (error == 0) {
		error = do_actual_lookup(ap->a_dvp, cnp, ap->a_vpp);
	}

	if (error != 0 || (cnp->cn_flags & SAVESTART) == 0)
		free(cnp->cn_pnbuf, M_NAMEI);

	vput(ap->a_dvp);
	return error;
}

static int
xfs_readlink(void *vap)
{
	struct vop_readlink_args *ap = vap;

	int			error = 0;

	XFSDEB(XDEBVNOPS, ("xfs_readlink\n"));

	error = xfs_data_valid(ap->a_vp, ap->a_cred, XFS_DATA_R);
	if (error == 0) {
		struct vnode *t = DATA_FROM_VNODE(ap->a_vp);

		vn_lock(t, LK_EXCLUSIVE | LK_RETRY, ap->a_uio->uio_procp);
		error = VOP_READ(t, ap->a_uio, 0, ap->a_cred);
		VOP_UNLOCK(t, 0, ap->a_uio->uio_procp);
	}

	return error;
}

static int
xfs_inactive(void *vap)
{
	struct vop_inactive_args *ap = vap;

	struct xfs_message_inactivenode msg;
	struct xfs		*xfsp = XFS_FROM_VNODE(ap->a_vp);
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_vp);

	XFSDEB(XDEBVNOPS, ("xfs_inactive, 0x%x\n", (int) ap->a_vp));

	xn->tokens = 0;
	msg.header.opcode = XFS_MSG_INACTIVENODE;
	msg.handle = xn->handle;
	msg.flag   = XFS_NOREFS;
	xfs_message_send(xfsp->fd, &msg.header, sizeof(msg));

	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);

	return 0;
}

static int
xfs_reclaim(void *vap)
{
	struct vop_reclaim_args *ap = vap;

	struct xfs_message_inactivenode msg;
	struct xfs		*xfsp = XFS_FROM_VNODE(ap->a_vp);
	struct xfs_node		*xn = VNODE_TO_XNODE(ap->a_vp);

	XFSDEB(XDEBVNOPS, ("xfs_reclaim, 0x%x\n", (int) ap->a_vp));

	msg.header.opcode = XFS_MSG_INACTIVENODE;
	msg.handle = xn->handle;
	msg.flag   = XFS_NOREFS | XFS_DELETE;
	xfs_message_send(xfsp->fd, &msg.header, sizeof(msg));

	cache_purge(ap->a_vp);
	free_xfs_node(xn);

	return 0;
}

static int
xfs_abortop (void *vap)
{
	struct vop_abortop_args	*ap = vap;
	struct componentname	*cnp = ap->a_cnp;

	XFSDEB(XDEBVNOPS, ("xfs_abortop\n"));
    
	if ((cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF) {
		FREE(cnp->cn_pnbuf, M_NAMEI);
		cnp->cn_pnbuf = NULL;
	}
	return 0;
}

static int
xfs_notsupp(void *vap)
{
	return EOPNOTSUPP;
}

int (**xfs_vnodeop_p) __P((void *));


static struct vnodeopv_entry_desc xfs_vnodeop_entries[] = {
	{&vop_default_desc,		vn_default_error},
	{&vop_lookup_desc,		xfs_lookup},
	{&vop_open_desc,		xfs_open},
	{&vop_fsync_desc,		xfs_fsync},
	{&vop_close_desc,		xfs_close},
	{&vop_read_desc,		xfs_read},
	{&vop_write_desc,		xfs_write},
	{&vop_mmap_desc,		xfs_notsupp},
	{&vop_bmap_desc,		xfs_notsupp},
	{&vop_ioctl_desc,		xfs_notsupp},
	{&vop_select_desc,		xfs_notsupp},
	{&vop_getattr_desc,		xfs_getattr},
	{&vop_setattr_desc,		xfs_setattr},
	{&vop_access_desc,		xfs_access},
	{&vop_create_desc,		xfs_create},
	{&vop_remove_desc,		xfs_remove},
	{&vop_link_desc,		xfs_link},
	{&vop_rename_desc,		xfs_rename},
	{&vop_mkdir_desc,		xfs_mkdir},
	{&vop_rmdir_desc,		xfs_rmdir},
	{&vop_readdir_desc,		xfs_readdir},
	{&vop_symlink_desc,		xfs_symlink},
	{&vop_readlink_desc,	xfs_readlink},
	{&vop_inactive_desc,	xfs_inactive},
	{&vop_reclaim_desc,		xfs_reclaim},
	{&vop_lock_desc,		vop_generic_lock},
	{&vop_unlock_desc,		vop_generic_unlock},
	{&vop_abortop_desc,		xfs_abortop},
	{(struct vnodeop_desc *) NULL, (int (*) __P((void *))) NULL}
};


struct vnodeopv_desc xfs_vnodeop_opv_desc =
		{&xfs_vnodeop_p, xfs_vnodeop_entries};
