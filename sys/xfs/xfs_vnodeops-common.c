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

#include <xfs/xfs_locl.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_common.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_deb.h>
#include <xfs/xfs_syscalls.h>
#include <xfs/xfs_vnodeops.h>

RCSID("$arla: xfs_vnodeops-common.c,v 1.94 2003/01/27 11:58:50 lha Exp $");

static void
xfs_handle_stale(struct xfs_node *xn)
{
    struct vnode *vp = XNODE_TO_VNODE(xn);

    if ((xn->flags & NNPFS_STALE) == 0)
	return;

#if __APPLE__
    if (UBCISVALID(vp) && !ubc_isinuse(vp, 1)) {
	xn->flags &= ~NNPFS_STALE;
	ubc_setsize(vp, 0);
	NNPFS_TOKEN_CLEAR(xn, ~0,
			NNPFS_OPEN_MASK | NNPFS_ATTR_MASK |
			NNPFS_DATA_MASK | NNPFS_LOCK_MASK);
    }
#endif
}

int
xfs_open_valid(struct vnode *vp, struct ucred *cred, d_thread_t *p,
	       u_int tok)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;

    NNPFSDEB(XDEBVFOPS, ("xfs_open_valid\n"));

    xfs_handle_stale(xn);

    do {
	if (!NNPFS_TOKEN_GOT(xn, tok)) {
	    struct xfs_message_open msg;

	    msg.header.opcode = NNPFS_MSG_OPEN;
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	    msg.handle = xn->handle;
	    msg.tokens = tok;

	    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);

	    if (error == 0)
		error = ((struct xfs_message_wakeup *) & msg)->error;
	} else {
	    goto done;
	}
    } while (error == 0);

done:
    NNPFSDEB(XDEBVFOPS, ("xfs_open_valid: error = %d\n", error));

    return error;
}

int
xfs_attr_valid(struct vnode *vp, struct ucred *cred, d_thread_t *p,
	       u_int tok)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;
    xfs_pag_t pag = xfs_get_pag(cred);

    do {
	if (!NNPFS_TOKEN_GOT(xn, tok) || !xfs_has_pag(xn, pag)) {
	    struct xfs_message_getattr msg;

	    msg.header.opcode = NNPFS_MSG_GETATTR;
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = pag;
	    msg.handle = xn->handle;
	    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);
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
xfs_data_valid(struct vnode *vp, struct ucred *cred, d_thread_t *p,
	       u_int tok, uint32_t want_offset)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;
    uint32_t offset;
    struct xfs_message_getdata msg;

    do {
	offset = want_offset;
	if (NNPFS_TOKEN_GOT(xn, tok|NNPFS_ATTR_R) && offset > xn->attr.va_size) {
	    offset = xn->attr.va_size;
	}
    
	NNPFSDEB(XDEBVNOPS, ("xfs_data_valid: offset: want %ld has %ld, "
			   "tokens: want %lx has %lx length: %ld\n",
			   (long) offset, (long) xn->offset,
			   (long) tok, (long) xn->tokens,
			   (long) xn->attr.va_size));

	if (NNPFS_TOKEN_GOT(xn, tok)) {
	    if (offset <= xn->offset || xn->attr.va_type == VDIR) {
		break;
	    }
	}

	msg.header.opcode = NNPFS_MSG_GETDATA;
	msg.cred.uid = cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cred);
	msg.handle = xn->handle;
	msg.tokens = tok;
	msg.offset = offset;
	
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);
	
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) & msg)->error;
	
    } while (error == 0);

    return error;
}

int
xfs_open_common(struct vnode *vp,
		int mode,
		struct ucred *cred,
		d_thread_t *p)
{
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int ret;

    NNPFSDEB(XDEBVNOPS, ("xfs_open\n"));

    if (mode & FWRITE) {
	ret = xfs_open_valid(vp, cred, p, NNPFS_OPEN_NW);
    } else {
	ret = xfs_open_valid(vp, cred, p, NNPFS_OPEN_NR);
    }

    /* always update the read cred */

    if (mode & FWRITE)
	xfs_update_write_cred(xn, cred);
    xfs_update_read_cred(xn, cred);

    return ret;
}

static int
do_fsync(struct xfs *xfsp,
	 struct xfs_node *xn,
	 struct ucred *cred,
	 d_thread_t *p,
	 u_int flag)
{
    int error;
    struct xfs_message_putdata msg;

    msg.header.opcode = NNPFS_MSG_PUTDATA;
    if (cred != NOCRED) {
	msg.cred.uid = cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cred);
    } else {
	msg.cred.uid = 0;
	msg.cred.pag = NNPFS_ANONYMOUSID;
    }
    msg.handle = xn->handle;
    vattr2xfs_attr(&xn->attr, &msg.attr);
    msg.flag   = flag;

    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);

    if (error == 0)
	error = ((struct xfs_message_wakeup *) & msg)->error;

    if (error == 0)
	xn->flags &= ~NNPFS_DATA_DIRTY;

    return error;
}

int
xfs_fsync_common(struct vnode *vp, struct ucred *cred,
		 int waitfor, d_thread_t *proc)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_fsync: %lx\n", (unsigned long)vp));

    /*
     * It seems that fsync is sometimes called after reclaiming a node.
     * In that case we just look happy.
     */

    if (xn == NULL) {
	printf("NNPFS PANIC WARNING! xfs_fsync called after reclaiming!\n");
	return 0;
    }
    
    xfs_pushdirty(vp, cred, proc);

    if (xn->flags & NNPFS_DATA_DIRTY) {
#ifdef FSYNC_RECLAIM
	/* writing back the data from this vnode failed */
	if (waitfor & FSYNC_RECLAIM) {
	    printf("xfs_fsync: data lost, failed to write back\n");
	    xn->flags &= ~NNPFS_DATA_DIRTY;
	    return 0;
	}
#endif    
	error = do_fsync(xfsp, xn, cred, proc, NNPFS_WRITE | NNPFS_FSYNC);
    }

    return error;
}

int
xfs_close_common(struct vnode *vp, int fflag,
		 d_thread_t *proc, struct ucred *cred)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;
    
    NNPFSDEB(XDEBVNOPS,
	   ("xfs_close cred = %lx, fflag = %x, xn->flags = %x\n",
	    (unsigned long)cred, fflag, xn->flags));

    if (vp->v_type == VREG)
	xfs_pushdirty(vp, cred, proc);

    if (fflag & FWRITE && xn->flags & NNPFS_DATA_DIRTY)
	error = do_fsync(xfsp, xn, cred, proc, NNPFS_WRITE);

    return error;
}

size_t
xfs_uio_end_length (struct uio *uio)
{
#if DIAGNOSTIC
    size_t sz = 0;
    int i;

    for (i = 0; i < uio->uio_iovcnt; i++)
	sz += uio->uio_iov[i].iov_len;
    if (sz != uio->uio_resid)
	panic("xfs_uio_end_length");
#endif
    return uio->uio_offset + uio->uio_resid;
}


int
xfs_read_common(struct vnode *vp, struct uio *uio, int ioflag,
		struct ucred *cred)
{
    int error = 0;
    int i;

    NNPFSDEB(XDEBVNOPS, ("xfs_read\n"));

    xfs_update_read_cred(VNODE_TO_XNODE(vp), cred);

#ifdef HAVE_FREEBSD_THREAD
    error = xfs_data_valid(vp, cred, xfs_uio_to_thread(uio), NNPFS_DATA_R,
			   xfs_uio_end_length(uio));
#else
    error = xfs_data_valid(vp, cred, xfs_uio_to_proc(uio), NNPFS_DATA_R,
			   xfs_uio_end_length(uio));
#endif

    NNPFSDEB(XDEBVNOPS, ("xfs_read: iovcnt: %d\n", uio->uio_iovcnt));
    for (i = 0; i < uio->uio_iovcnt; i++)
	NNPFSDEB(XDEBVNOPS, ("  base: %lx len: %lu\n",
			   (unsigned long)uio->uio_iov[i].iov_base,
			   (unsigned long)uio->uio_iov[i].iov_len));

    if (error == 0) {
	struct vnode *t = DATA_FROM_VNODE(vp);

#ifdef HAVE_FREEBSD_THREAD
	xfs_vfs_readlock(t, xfs_uio_to_thread(uio));
	xfs_vop_read(t, uio, ioflag, cred, error);
	xfs_vfs_unlock(t, xfs_uio_to_thread(uio));
#else
	xfs_vfs_readlock(t, xfs_uio_to_proc(uio));
	xfs_vop_read(t, uio, ioflag, cred, error);
	xfs_vfs_unlock(t, xfs_uio_to_proc(uio));
#endif
    }

    NNPFSDEB(XDEBVNOPS, ("xfs_read offset: %lu resid: %lu\n",
		       (unsigned long)uio->uio_offset,
		       (unsigned long)uio->uio_resid));
    NNPFSDEB(XDEBVNOPS, ("xfs_read error: %d\n", error));

    return error;
}

int
xfs_write_common(struct vnode *vp, struct uio *uiop, int ioflag,
		 struct ucred *cred)
{
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_write\n"));

    xfs_update_write_cred(xn, cred);

#ifdef HAVE_FREEBSD_THREAD
    error = xfs_data_valid(vp, cred, xfs_uio_to_thread(uiop), NNPFS_DATA_W,
			   VNODE_TO_XNODE(vp)->attr.va_size);
#else
    error = xfs_data_valid(vp, cred, xfs_uio_to_proc(uiop), NNPFS_DATA_W,
			   VNODE_TO_XNODE(vp)->attr.va_size);
#endif

    if (error == 0) {
	struct vnode *t = DATA_FROM_XNODE(xn);
	struct vattr sub_attr;
	int error2 = 0;
 
 #ifdef HAVE_FREEBSD_THREAD
	xfs_vfs_writelock(t, xfs_uio_to_thread(uiop));
	xfs_vop_write(t, uiop, ioflag, cred, error);
	VNODE_TO_XNODE(vp)->flags |= NNPFS_DATA_DIRTY;
	xfs_vop_getattr(t, &sub_attr, cred, xfs_uio_to_thread(uiop), error2);
 #else
	xfs_vfs_writelock(t, xfs_uio_to_proc(uiop));
	xfs_vop_write(t, uiop, ioflag, cred, error);
	VNODE_TO_XNODE(vp)->flags |= NNPFS_DATA_DIRTY;
	xfs_vop_getattr(t, &sub_attr, cred, xfs_uio_to_proc(uiop), error2);
 #endif

	if (error2 == 0) {
	    xn->attr.va_size  = sub_attr.va_size;
	    xn->attr.va_bytes = sub_attr.va_size;
	    xn->attr.va_mtime = sub_attr.va_mtime;
	    xfs_set_vp_size(vp, sub_attr.va_size);
	    xn->offset = sub_attr.va_size;
	}
#ifdef HAVE_FREEBSD_THREAD
	xfs_vfs_unlock(t, xfs_uio_to_thread(uiop));
#else
	xfs_vfs_unlock(t, xfs_uio_to_proc(uiop));
#endif
    }

    return error;
}

int
xfs_getattr_common(struct vnode *vp, struct vattr *vap,
		   struct ucred *cred, d_thread_t *p)
{
    int error = 0;

    struct xfs_node *xn = VNODE_TO_XNODE(vp);

    NNPFSDEB(XDEBVNOPS, ("xfs_getattr\n"));

    error = xfs_attr_valid(vp, cred, p, NNPFS_ATTR_R);
    if (error == 0)
	*vap = xn->attr;
    return error;
}

int
xfs_setattr_common(struct vnode *vp, struct vattr *vap,
		   struct ucred *cred, d_thread_t *p)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_setattr\n"));

#define CHECK_NNPFSATTR(A, cast) (vap->A == cast VNOVAL || vap->A == xn->attr.A)
	if (CHECK_NNPFSATTR(va_mode,(mode_t)) &&
	    CHECK_NNPFSATTR(va_nlink,(short)) &&
	    CHECK_NNPFSATTR(va_size,(va_size_t)) &&
	    CHECK_NNPFSATTR(va_uid,(uid_t)) &&
	    CHECK_NNPFSATTR(va_gid,(gid_t)) &&
	    CHECK_NNPFSATTR(va_mtime.tv_sec,(unsigned int)) &&
	    CHECK_NNPFSATTR(va_fileid,(long)) &&
	    CHECK_NNPFSATTR(va_type,(enum vtype)))
		return 0;		/* Nothing to do */
#undef CHECK_NNPFSATTR

    if (NNPFS_TOKEN_GOT(xn, NNPFS_ATTR_W)) {
	/* Update attributes and mark them dirty. */
	VNODE_TO_XNODE(vp)->flags |= NNPFS_ATTR_DIRTY;
	error = EINVAL;		       /* XXX not yet implemented */
	goto done;
    } else {
	struct xfs_message_putattr msg;

	msg.header.opcode = NNPFS_MSG_PUTATTR;
	if (cred != NOCRED) {
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	} else {
	    msg.cred.uid = 0;
	    msg.cred.pag = NNPFS_ANONYMOUSID;
	}
	msg.handle = xn->handle;
	vattr2xfs_attr(vap, &msg.attr);
	if (NNPFS_TOKEN_GOT(xn, NNPFS_DATA_R)) {
	    if (vp->v_type == VREG) {
		if (vap->va_size != (va_size_t)VNOVAL)
		    XA_SET_SIZE(&msg.attr, vap->va_size);
		else
		    XA_SET_SIZE(&msg.attr, xn->attr.va_size);
#ifdef __APPLE__
		/* XXX needed ? */
		if (UBCINFOEXISTS(vp))
		    ubc_setsize(vp, msg.attr.xa_size);
#endif
	    }
	    if (vap->va_mtime.tv_sec != (unsigned int)VNOVAL)
		XA_SET_MTIME(&msg.attr, vap->va_mtime.tv_sec);
	    else
		XA_SET_MTIME(&msg.attr, xn->attr.va_mtime.tv_sec);
	}

	NNPFS_TOKEN_CLEAR(xn, NNPFS_ATTR_VALID, NNPFS_ATTR_MASK);
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);
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
	if ((rights & NNPFS_RIGHT_R) == 0)
	    error = EACCES;
    if (mode & VWRITE)
	if ((rights & NNPFS_RIGHT_W) == 0)
	    error = EACCES;
    if (mode & VEXEC)
	if ((rights & NNPFS_RIGHT_X) == 0)
	    error = EACCES;
    return error;
}

int
xfs_access_common(struct vnode *vp, int mode, struct ucred *cred,
		  d_thread_t *p)
{
    int error = 0;
    xfs_pag_t pag = xfs_get_pag(cred);

    NNPFSDEB(XDEBVNOPS, ("xfs_access mode = 0%o\n", mode));

    error = xfs_attr_valid(vp, cred, p, NNPFS_ATTR_R);
    if (error == 0) {
	struct xfs_node *xn = VNODE_TO_XNODE(vp);
	int i;

	error = check_rights (xn->anonrights, mode);

	if (error == 0)
	    goto done;

	NNPFSDEB(XDEBVNOPS, ("xfs_access anonaccess failed\n"));

	error = EACCES;		/* default to EACCES if pag isn't in xn->id */

	for (i = 0; i < MAXRIGHTS; i++)
	    if (xn->id[i] == pag) {
		error = check_rights (xn->rights[i], mode);
		break;
	    }
    }

done:
    NNPFSDEB(XDEBVNOPS, ("xfs_access(0%o) = %d\n", mode, error));

    return error;
}

int
xfs_lookup_common(struct vnode *dvp, 
		  xfs_componentname *cnp, 
		  struct vnode **vpp)
{
    struct xfs_message_getnode msg;
    struct xfs *xfsp = NNPFS_FROM_VNODE(dvp);
    struct xfs_node *d = VNODE_TO_XNODE(dvp);
    int error = 0;
#ifdef HAVE_FREEBSD_THREAD
    d_thread_t *proc  = xfs_cnp_to_thread(cnp);
    struct ucred *cred = xfs_thread_to_cred(proc);
#else
    d_thread_t *proc  = xfs_cnp_to_proc(cnp);
    struct ucred *cred = xfs_proc_to_cred(proc);
#endif

    NNPFSDEB(XDEBVNOPS, ("xfs_lookup_common: enter\n"));

    *vpp = NULL;

    if (cnp->cn_namelen >= NNPFS_MAX_NAME)
	return ENAMETOOLONG;
	
    if (dvp->v_type != VDIR)
	return ENOTDIR;

    if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
	*vpp = dvp;
	VREF(*vpp);
	return 0;
    }
    
    do {
	xfs_vop_access(dvp, VEXEC, cred, proc, error);
	if (error != 0)
	    goto done;

	NNPFSDEB(XDEBVNOPS, ("xfs_lookup_common: dvp = %lx\n",
			   (unsigned long) dvp));
	NNPFSDEB(XDEBVNOPS, ("xfs_lookup_common: cnp = %lx, "
			   "cnp->cn_nameiop = %d\n", 
			   (unsigned long) cnp, (int)cnp->cn_nameiop));
	

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

	    msg.header.opcode = NNPFS_MSG_GETNODE;
	    if (cnp->cn_cred != NOCRED) {
		msg.cred.uid = cnp->cn_cred->cr_uid;
		msg.cred.pag = xfs_get_pag(cnp->cn_cred);
	    } else {
		msg.cred.uid = 0;
		msg.cred.pag = NNPFS_ANONYMOUSID;
	    }
	    msg.parent_handle = d->handle;
	    memcpy(msg.name, cnp->cn_nameptr, cnp->cn_namelen);
	    msg.name[cnp->cn_namelen] = '\0';
	    error = xfs_message_rpc(xfsp->fd, &msg.header,
				    sizeof(msg), proc);
	    if (error == 0)
		error = ((struct xfs_message_wakeup *) & msg)->error;
	    if(error == ENOENT && cnp->cn_nameiop != CREATE) {
		NNPFSDEB(XDEBVNOPS, ("xfs_lookup: neg cache %lx (%s, %ld)\n",
				   (unsigned long)dvp,
				   cnp->cn_nameptr, cnp->cn_namelen));
		xfs_dnlc_enter (dvp, cnp, NULL);
	    }
	} else if (error == -1) {
	    error = 0;
	    goto done;
	}
    } while (error == 0);

done:
    NNPFSDEB(XDEBVNOPS, ("xfs_lookup_common: return error = %d\n", error));
    return error;
}

int
xfs_create_common(struct vnode *dvp,
		  const char *name,
		  struct vattr *vap, 
		  struct ucred *cred,
		  d_thread_t *p)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_create: (%lx, %s)\n",
		       (unsigned long)dvp, name));
    {
	struct xfs_message_create msg;

	msg.header.opcode = NNPFS_MSG_CREATE;
	msg.parent_handle = xn->handle;
	if (strlcpy(msg.name, name, sizeof(msg.name)) >= NNPFS_MAX_NAME)
	    return ENAMETOOLONG;
	vattr2xfs_attr(vap, &msg.attr);

	msg.mode = 0;		       /* XXX - mode */
	if (cred != NOCRED) {
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	} else {
	    msg.cred.uid = 0;
	    msg.cred.pag = NNPFS_ANONYMOUSID;
	}


	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);

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
		  struct ucred *cred,
		  d_thread_t *p)
{
    struct xfs *xfsp  = NNPFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    struct xfs_message_remove msg;
    int error;

    NNPFSDEB(XDEBVNOPS, ("xfs_remove: %s\n", name));

    msg.header.opcode = NNPFS_MSG_REMOVE;
    msg.parent_handle = xn->handle;
    msg.cred.uid = cred->cr_uid;
    msg.cred.pag = xfs_get_pag(cred);
    
    if (strlcpy(msg.name, name, sizeof(msg.name)) >= NNPFS_MAX_NAME)
	error = ENAMETOOLONG;
    else
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);
    if (error == 0)
	error = ((struct xfs_message_wakeup *) &msg)->error;

    if (error == 0)
	xfs_dnlc_purge (vp);

    return error;
}

int
xfs_rename_common(struct vnode *fdvp, 
		  struct vnode *fvp,
		  const char *fname,
		  struct vnode *tdvp,
		  struct vnode *tvp,
		  const char *tname,
		  struct ucred *cred,
		  d_thread_t *p)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(fdvp);
    int error;

    NNPFSDEB(XDEBVNOPS, ("xfs_rename: %s %s\n", fname, tname));

    if ((fvp->v_mount != tdvp->v_mount)
	|| (tvp && (fvp->v_mount != tvp->v_mount))) {
	return  EXDEV;
    }

    {
	struct xfs_message_rename msg;

	msg.header.opcode = NNPFS_MSG_RENAME;
	msg.old_parent_handle = VNODE_TO_XNODE(fdvp)->handle;
	if (strlcpy(msg.old_name, fname, sizeof(msg.old_name)) >= NNPFS_MAX_NAME)
	    return ENAMETOOLONG;
	msg.new_parent_handle = VNODE_TO_XNODE(tdvp)->handle;
	if (strlcpy(msg.new_name, tname, sizeof(msg.new_name)) >= NNPFS_MAX_NAME)
	    return ENAMETOOLONG;
	msg.cred.uid = cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cred);
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) &msg)->error;

    }

    NNPFSDEB(XDEBVNOPS, ("xfs_rename: error = %d\n", error));

    return error;
}

int
xfs_mkdir_common(struct vnode *dvp, 
		 const char *name,
		 struct vattr *vap, 
		 struct ucred *cred,
		 d_thread_t *p)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_mkdir: %s\n", name));
    {
	struct xfs_message_mkdir msg;

	msg.header.opcode = NNPFS_MSG_MKDIR;
	msg.parent_handle = xn->handle;
	if (strlcpy(msg.name, name, sizeof(msg.name)) >= NNPFS_MAX_NAME)
	    return ENAMETOOLONG;
	vattr2xfs_attr(vap, &msg.attr);
	if (cred != NOCRED) {
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	} else {
	    msg.cred.uid = 0;
	    msg.cred.pag = NNPFS_ANONYMOUSID;
	}
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) & msg)->error;
    }

    return error;
}

int
xfs_rmdir_common(struct vnode *dvp,
		 struct vnode *vp,
		 const char *name,
		 struct ucred *cred,
		 d_thread_t *p)
{
    struct xfs *xfsp  = NNPFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    struct xfs_message_rmdir msg;
    int error;

    NNPFSDEB(XDEBVNOPS, ("xfs_rmdir: %s\n", name));

    msg.header.opcode = NNPFS_MSG_RMDIR;
    msg.parent_handle = xn->handle;
    msg.cred.uid = cred->cr_uid;
    msg.cred.pag = xfs_get_pag(cred);
    if (strlcpy(msg.name, name, sizeof(msg.name)) >= NNPFS_MAX_NAME)
	error = ENAMETOOLONG;
    else
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);
    if (error == 0)
	error = ((struct xfs_message_wakeup *) &msg)->error;

    if (error == 0)
	xfs_dnlc_purge (vp);

    NNPFSDEB(XDEBVNOPS, ("xfs_rmdir error: %d\n", error));

    return error;
}

int
xfs_readdir_common(struct vnode *vp, 
		   struct uio *uiop, 
		   struct ucred *cred,
		   d_thread_t *p,
		   int *eofflag)
{
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_readdir\n"));

    if(eofflag)
	*eofflag = 0;
#ifdef HAVE_FREEBSD_THREAD
    error = xfs_data_valid(vp, cred, xfs_uio_to_thread(uiop), NNPFS_DATA_R,
			   xfs_uio_end_length(uiop));
#else
    error = xfs_data_valid(vp, cred, xfs_uio_to_proc(uiop), NNPFS_DATA_R,
			   xfs_uio_end_length(uiop));
#endif
    if (error == 0) {
	struct vnode *t = DATA_FROM_VNODE(vp);

#ifdef HAVE_FREEBSD_THREAD
	xfs_vfs_readlock(t, xfs_uio_to_thread(uiop));
#else
	xfs_vfs_readlock(t, xfs_uio_to_proc(uiop));
#endif
	xfs_vop_read(t, uiop, 0, cred, error);
	if (eofflag) {
	    struct vattr t_attr;
	    int error2;

#ifdef HAVE_FREEBSD_THREAD
	    xfs_vop_getattr(t, &t_attr, cred, xfs_uio_to_thread(uiop), error2);
#else
	    xfs_vop_getattr(t, &t_attr, cred, xfs_uio_to_proc(uiop), error2);
#endif
	    if (error2 == 0)
		*eofflag = t_attr.va_size <= uiop->uio_offset;
	}
#ifdef HAVE_FREEBSD_THREAD
	xfs_vfs_unlock(t, xfs_uio_to_thread(uiop));
#else
	xfs_vfs_unlock(t, xfs_uio_to_proc(uiop));
#endif
    }
    return error;
}

int
xfs_link_common(struct vnode *dvp, 
		struct vnode *vp, 
		const char *name,
		struct ucred *cred,
		d_thread_t *p)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    struct xfs_node *xn2 = VNODE_TO_XNODE(vp);
    struct xfs_message_link msg;
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_link: %s\n", name));
    
    msg.header.opcode = NNPFS_MSG_LINK;
    msg.parent_handle = xn->handle;
    msg.from_handle   = xn2->handle;
    if (strlcpy(msg.name, name, sizeof(msg.name)) >= NNPFS_MAX_NAME)
	return ENAMETOOLONG;
    msg.cred.uid = cred->cr_uid;
    msg.cred.pag = xfs_get_pag(cred);

    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);
    if (error == 0)
	error = ((struct xfs_message_wakeup *) & msg)->error;
    
    return error;
}

int
xfs_symlink_common(struct vnode *dvp,
		   struct vnode **vpp,
		   xfs_componentname *cnp,
		   struct vattr *vap,
		   char *target)
{
    struct xfs *xfsp = NNPFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
#ifdef HAVE_FREEBSD_THREAD
    d_thread_t *proc  = xfs_cnp_to_thread(cnp);
    struct ucred *cred = xfs_thread_to_cred(proc);
#else
    d_thread_t *proc  = xfs_cnp_to_proc(cnp);
    struct ucred *cred = xfs_proc_to_cred(proc);
#endif
    struct xfs_message_symlink msg;
    const char *name = cnp->cn_nameptr;
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_symlink: %s\n", name));

    msg.header.opcode = NNPFS_MSG_SYMLINK;
    msg.parent_handle = xn->handle;
    vattr2xfs_attr(vap, &msg.attr);
    msg.cred.uid = cred->cr_uid;
    msg.cred.pag = xfs_get_pag(cred);
    if (strlcpy (msg.contents, target, sizeof(msg.contents)) >= NNPFS_MAX_SYMLINK_CONTENT) {
	error = ENAMETOOLONG;
	goto done;
    }
    if (strlcpy(msg.name, name, sizeof(msg.name)) >= NNPFS_MAX_NAME) {
	error = ENAMETOOLONG;
	goto done;
    }
    error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), proc);
    if (error == 0)
	error = ((struct xfs_message_wakeup *) & msg)->error;

 done:
    return error;
}

int
xfs_readlink_common(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_readlink\n"));

#ifdef HAVE_FREEBSD_THREAD
    error = xfs_data_valid(vp, cred, xfs_uio_to_thread(uiop), NNPFS_DATA_R,
			   xfs_uio_end_length(uiop));
#else
    error = xfs_data_valid(vp, cred, xfs_uio_to_proc(uiop), NNPFS_DATA_R,
			   xfs_uio_end_length(uiop));
#endif
    if (error == 0) {
	struct vnode *t = DATA_FROM_VNODE(vp);

#ifdef HAVE_FREEBSD_THREAD
	xfs_vfs_readlock(t, xfs_uio_to_thread(uiop));
	xfs_vop_read(t, uiop, 0, cred, error);
	xfs_vfs_unlock(t, xfs_uio_to_thread(uiop));
#else
	xfs_vfs_readlock(t, xfs_uio_to_proc(uiop));
	xfs_vop_read(t, uiop, 0, cred, error);
	xfs_vfs_unlock(t, xfs_uio_to_proc(uiop));
#endif
    }
    return error;
}

int
xfs_inactive_common(struct vnode *vp, d_thread_t *p)
{
    int error;
    struct xfs_node *xn = VNODE_TO_XNODE(vp);

    NNPFSDEB(XDEBVNOPS, ("xfs_inactive, %lx\n",
		       (unsigned long)vp));

    /*
     * This seems rather bogus, but sometimes we get an already
     * cleaned node to be made inactive.  Just ignoring it seems safe.
     */

    if (xn == NULL) {
	NNPFSDEB(XDEBVNOPS, ("xfs_inactive: clean node\n"));
	return 0;
    }

    /* xn->wr_cred not set -> NOCRED */

    if (vp->v_type == VREG)
	xfs_pushdirty(vp, xn->wr_cred, p);

    error = xfs_fsync_common(vp, xn->wr_cred, /* XXX */ 0, p);
    if (error) {
	printf ("xfs_inactive: failed writing back data: %d\n", error);
	xn->flags &= ~NNPFS_DATA_DIRTY;
    }

    /* If this node is no longer valid, recycle immediately. */
    if (!NNPFS_TOKEN_GOT(xn, NNPFS_ATTR_R | NNPFS_ATTR_W)
	|| (xn->flags & NNPFS_STALE) == NNPFS_STALE)
    {
#ifndef __osf__
	xfs_vfs_unlock(vp, p);
        NNPFSDEB(XDEBVNOPS, ("xfs_inactive: vrecycle\n"));
        vrecycle(vp, 0, p);
#else /* __osf__ */
	NNPFSDEB(XDEBVNOPS, ("xfs_inactive: vp = %lx vp->v_usecount= %d\n",
			     (unsigned long)vp, vp?vp->v_usecount:0));
#endif /* __osf__ */
    } else {
#ifndef __osf__
	xfs_vfs_unlock(vp, p);
#endif
	xn->flags &= ~NNPFS_STALE;
    }

    NNPFSDEB(XDEBVNOPS, ("return: xfs_inactive\n"));

    return 0;
}

int
xfs_reclaim_common(struct vnode *vp)
{
    struct xfs_message_inactivenode msg;
    struct xfs *xfsp = NNPFS_FROM_VNODE(vp);
    struct xfs_node *xn = VNODE_TO_XNODE(vp);

    NNPFSDEB(XDEBVNOPS, ("xfs_reclaim: %lx\n",
		       (unsigned long)vp));

    NNPFS_TOKEN_CLEAR(xn,
		    ~0,
		    NNPFS_OPEN_MASK | NNPFS_ATTR_MASK |
		    NNPFS_DATA_MASK | NNPFS_LOCK_MASK);
    /* Release, data if we still have it. */
    if (DATA_FROM_XNODE(xn) != 0) {
        vrele(DATA_FROM_XNODE(xn));
	DATA_FROM_XNODE(xn) = 0;
    }

    xfs_remove_node(&xfsp->nodehead, xn);

    msg.header.opcode = NNPFS_MSG_INACTIVENODE;
    msg.handle = xn->handle;
    msg.flag   = NNPFS_NOREFS | NNPFS_DELETE;
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
    struct xfs *xfsp = NNPFS_FROM_VNODE(dvp);
    struct xfs_node *xn = VNODE_TO_XNODE(dvp);
    int error = 0;

    NNPFSDEB(XDEBVNOPS, ("xfs_advlock\n"));
    {
	struct xfs_message_advlock msg;

	msg.header.opcode = NNPFS_MSG_ADVLOCK;
	msg.handle = xn->handle;
	msg.locktype = locktype;
	msg.lockid = lockid;

	if (cred != NOCRED) {
	    msg.cred.uid = cred->cr_uid;
	    msg.cred.pag = xfs_get_pag(cred);
	} else {
	    msg.cred.uid = 0;
	    msg.cred.pag = NNPFS_ANONYMOUSID;
	}
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg), p);
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
	    NNPFS_TOKEN_GOT(xn, NNPFS_ATTR_VALID) ? "": "in");
    printf ("\tdata: %svalid\n", 
	    NNPFS_TOKEN_GOT(xn, NNPFS_DATA_VALID) ? "": "in");
    printf ("\tflags: 0x%x\n", xn->flags);
    printf ("\toffset: %d\n", xn->offset);
}
