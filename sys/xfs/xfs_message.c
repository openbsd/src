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

#include <xfs/xfs_locl.h>
#include <xfs/xfs_deb.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_msg_locl.h>
#include <xfs/xfs_syscalls.h>
#include <xfs/xfs_vfsops.h>
#include <xfs/xfs_vnodeops.h>
#include <xfs/xfs_dev.h>

RCSID("$arla: xfs_message.c,v 1.84 2003/06/02 18:25:20 lha Exp $");

static void
send_inactive_node(int fd, xfs_handle *handle)
{
    struct xfs_message_inactivenode msg;
    
    msg.header.opcode = NNPFS_MSG_INACTIVENODE;
    msg.handle = *handle;
    msg.flag   = NNPFS_NOREFS | NNPFS_DELETE;
    xfs_message_send(fd, &msg.header, sizeof(msg));
}


int
xfs_message_installroot(int fd,
			struct xfs_message_installroot * message,
			u_int size,
			d_thread_t *p)
{
    int error = 0;

    NNPFSDEB(XDEBMSG, ("xfs_message_installroot (%d,%d,%d,%d)\n",
		     message->node.handle.a,
		     message->node.handle.b,
		     message->node.handle.c,
		     message->node.handle.d));

    if (xfs[fd].root != NULL) {
	printf("NNPFS PANIC WARNING! xfs_message_installroot: called again!\n");
	error = EBUSY;
    } else {
	error = new_xfs_node(&xfs[fd], &message->node, &xfs[fd].root, p);
	if (error)
	    return error;
	NNPFS_MAKE_VROOT(xfs[fd].root->vn);
    }
    return error;
}

int
xfs_message_installnode(int fd,
			struct xfs_message_installnode * message,
			u_int size,
			d_thread_t *p)
{
    int error = 0;
    struct xfs_node *n, *dp;

    NNPFSDEB(XDEBMSG, ("xfs_message_installnode (%d,%d,%d,%d)\n",
		     message->node.handle.a,
		     message->node.handle.b,
		     message->node.handle.c,
		     message->node.handle.d));

retry:
    dp = xfs_node_find(&xfs[fd].nodehead, &message->parent_handle);
    if (dp) {
	struct vnode *t_vnode = XNODE_TO_VNODE(dp);

	NNPFSDEB(XDEBMSG, ("xfs_message_installnode: t_vnode = %lx\n",
			   (unsigned long)t_vnode));

	if (xfs_do_vget(t_vnode, 0 /* LK_SHARED */, p))
		goto retry;

	error = new_xfs_node(&xfs[fd], &message->node, &n, p);
	if (error) {
	    vrele (t_vnode);
	    return error;
	}

	xfs_dnlc_enter_name(t_vnode,
			    message->name,
			    XNODE_TO_VNODE(n));
	vrele (XNODE_TO_VNODE(n));
	vrele (t_vnode);
    } else {
	printf("NNPFS PANIC WARNING! xfs_message_installnode: no parent\n");
	error = ENOENT;
    }
    NNPFSDEB(XDEBMSG, ("return: xfs_message_installnode: %d\n", error));

    return error;
}

int
xfs_message_installattr(int fd,
			struct xfs_message_installattr * message,
			u_int size,
			d_thread_t *p)
{
    int error = 0;
    struct xfs_node *t;

    NNPFSDEB(XDEBMSG, ("xfs_message_installattr (%d,%d,%d,%d) \n",
		     message->node.handle.a,
		     message->node.handle.b,
		     message->node.handle.c,
		     message->node.handle.d));

    t = xfs_node_find(&xfs[fd].nodehead, &message->node.handle);
    if (t != 0) {
	t->tokens = message->node.tokens;
	if ((t->tokens & NNPFS_DATA_MASK) && DATA_FROM_XNODE(t) == NULL) {
	    printf ("xfs_message_installattr: tokens and no data\n");
	    t->tokens &= ~NNPFS_DATA_MASK;
	}
	xfs_attr2vattr(&message->node.attr, &t->attr, 0);
	if ((t->flags & NNPFS_VMOPEN) == 0)
	    xfs_set_vp_size(XNODE_TO_VNODE(t), t->attr.va_size);
	bcopy(message->node.id, t->id, sizeof(t->id));
	bcopy(message->node.rights, t->rights, sizeof(t->rights));
	t->anonrights = message->node.anonrights;
    } else {
	NNPFSDEB(XDEBMSG, ("xfs_message_installattr: no such node\n"));
    }
    
    return error;
}

int
xfs_message_installdata(int fd,
			struct xfs_message_installdata * message,
			u_int size,
			d_thread_t *p)
{
    struct xfs_node *t;
    int error = 0;

    NNPFSDEB(XDEBMSG, ("xfs_message_installdata (%d,%d,%d,%d)\n",
		     message->node.handle.a,
		     message->node.handle.b,
		     message->node.handle.c,
		     message->node.handle.d));

retry:
    t = xfs_node_find(&xfs[fd].nodehead, &message->node.handle);
    if (t != NULL) {
	struct xfs_fhandle_t *fh = 
	    (struct xfs_fhandle_t *)&message->cache_handle;
	struct vnode *t_vnode = XNODE_TO_VNODE(t);
	struct vnode *vp;

	message->cache_name[sizeof(message->cache_name)-1] = '\0';
	NNPFSDEB(XDEBMSG, ("cache_name = '%s'\n", message->cache_name));

	if (xfs_do_vget(t_vnode, 0 /* LK_SHARED */, p))
		goto retry;

	if (message->flag & NNPFS_ID_HANDLE_VALID) {
	    error = xfs_fhlookup (p, fh, &vp);
	} else {
	    error = EINVAL;
	}
	if (error != 0) {
#ifdef __osf__
	    struct nameidata *ndp = &u.u_nd;
#else
	    struct nameidata nd;
	    struct nameidata *ndp = &nd;
#endif

	    NNPFSDEB(XDEBMSG,
		   ("xfs_message_installdata: fhlookup failed: %d, "
		    "opening by name\n", error));

	    NDINIT(ndp, LOOKUP, FOLLOW | NNPFS_LOCKLEAF, UIO_SYSSPACE,
		   message->cache_name, p);
	    error = namei(ndp);
	    vp = ndp->ni_vp;
	}

	if (error == 0) {
#ifndef __osf__
	    xfs_vfs_unlock(vp, p);
#endif
	    if (DATA_FROM_XNODE(t))
		vrele(DATA_FROM_XNODE(t));
	    DATA_FROM_XNODE(t) = vp;

	    NNPFSDEB(XDEBMSG, ("xfs_message_installdata: t = %lx;"
			     " tokens = %x\n",
			     (unsigned long)t, message->node.tokens));

	    t->tokens = message->node.tokens;
	    xfs_attr2vattr(&message->node.attr, &t->attr, 1);
	    if ((t->flags & NNPFS_VMOPEN) == 0)
		xfs_set_vp_size(XNODE_TO_VNODE(t), t->attr.va_size);
	    if (XNODE_TO_VNODE(t)->v_type == VDIR
		&& (message->flag & NNPFS_ID_INVALID_DNLC))
		xfs_dnlc_purge (XNODE_TO_VNODE(t));
	    bcopy(message->node.id, t->id, sizeof(t->id));
	    bcopy(message->node.rights, t->rights, sizeof(t->rights));
	    t->anonrights = message->node.anonrights;
	    t->offset = message->offset;
#if 0
	    if (message->flag & NNPFS_ID_AFSDIR)
		t->flags |= NNPFS_AFSDIR;
#endif
	} else {
	    printf("NNPFS PANIC WARNING! xfs_message_installdata failed!\n");
	    printf("Reason: lookup failed on cache file '%s', error = %d\n",
		   message->cache_name, error);
	}
	vrele (t_vnode);
    } else {
	printf("NNPFS PANIC WARNING! xfs_message_installdata failed\n");
	printf("Reason: No node to install the data into!\n");
	error = ENOENT;
    }

    return error;
}

#ifdef __osf__
#define xfs_writecount v_wrcnt
#else
#define xfs_writecount v_writecount
#endif

int
xfs_message_invalidnode(int fd,
			struct xfs_message_invalidnode * message,
			u_int size,
			d_thread_t *p)
{
    int error = 0;
    struct xfs_node *t;

    NNPFSDEB(XDEBMSG, ("xfs_message_invalidnode (%d,%d,%d,%d)\n",
		     message->handle.a,
		     message->handle.b,
		     message->handle.c,
		     message->handle.d));

#ifdef __APPLE__
 retry:
#endif
    t = xfs_node_find(&xfs[fd].nodehead, &message->handle);
    if (t != 0) {
	struct vnode *vp = XNODE_TO_VNODE(t);

        /* If open for writing, return immediately. Last close:er wins! */
	if (vp->v_usecount >= 0 && vp->xfs_writecount >= 1)
            return 0;

#ifdef __FreeBSD__
	{
	    vm_object_t obj = vp->v_object;

	    if (obj != NULL
		&& (obj->ref_count != 0
#ifdef OBJ_MIGHTBEDIRTY
		|| (obj->flags & OBJ_MIGHTBEDIRTY) != 0
#endif
		    ))
		return 0;

	}
#endif /* __FreeBSD__ */

	/* If node is in use, mark as stale */
	if (vp->v_usecount > 0 && vp->v_type != VDIR) {
#ifdef __APPLE__
	    if (vget(vp, 0, p))
		goto retry;

	    if (UBCISVALID(vp) && !ubc_isinuse(vp, 1)) {
		ubc_setsize(vp, 0);
		vrele(vp);
	    } else {
		vrele(vp);
		t->flags |= NNPFS_STALE;
		return 0;
	    }
#else
	    t->flags |= NNPFS_STALE;
	    return 0;
#endif
	}

	if (DATA_FROM_XNODE(t)) {
	    vrele(DATA_FROM_XNODE(t));
	    DATA_FROM_XNODE(t) = (struct vnode *) 0;
	}
	NNPFS_TOKEN_CLEAR(t, ~0,
			NNPFS_OPEN_MASK | NNPFS_ATTR_MASK |
			NNPFS_DATA_MASK | NNPFS_LOCK_MASK);
	/* Dir changed, must invalidate DNLC. */
	if (vp->v_type == VDIR)
	    xfs_dnlc_purge(vp);
	if (vp->v_usecount == 0) {
#ifndef __osf__
	    NNPFSDEB(XDEBVNOPS, ("xfs_message_invalidnode: vrecycle\n"));
	    vrecycle(vp, 0, p);
#else
	    /* XXX */
#endif /* __osf__ */
	}
    } else {
	NNPFSDEB(XDEBMSG, ("xfs_message_invalidnode: no such node\n"));
	send_inactive_node(fd, &message->handle);
	error = ENOENT;
    }

    return error;
}

int
xfs_message_updatefid(int fd,
		      struct xfs_message_updatefid * message,
		      u_int size,
		      d_thread_t *p)
{
    int error = 0;

    NNPFSDEB(XDEBMSG, ("xfs_message_updatefid (%d,%d,%d,%d) (%d,%d,%d,%d)\n",
		       message->old_handle.a,
		       message->old_handle.b,
		       message->old_handle.c,
		       message->old_handle.d,
		       message->new_handle.a,
		       message->new_handle.b,
		       message->new_handle.c,
		       message->new_handle.d));

    error = xfs_update_handle(&xfs[fd].nodehead, 
				&message->old_handle,
				&message->new_handle);
    if (error)
	printf ("NNPFS PANIC WARNING! xfs_message_updatefid: %d\n", error);
    return error;
}

#if __osf__

/*
 * Try to clean out nodes for the userland daemon
 */

static void
gc_vnode (struct vnode *vp,
	  d_thread_t *p)
{
    /* This node is on the freelist */
    if (vp->v_usecount <= 0) {
	
	/*  DIAGNOSTIC */
	if (vp->v_usecount < 0) {
		    vprint("vrele: bad ref count", vp);
		    panic("vrele: ref cnt");
	}
	
	NNPFSDEB(XDEBMSG, ("xfs_message_gc: success\n"));
	
	vgone(vp, VX_NOSLEEP, NULL);
    } else {
	NNPFSDEB(XDEBMSG, ("xfs_message_gc: used\n"));
    }

}


#else /* !__osf__ */

/*
 * Try to clean out nodes for the userland daemon
 */

static void
gc_vnode (struct vnode *vp,
	  d_thread_t *p)
{
#ifdef HAVE_SYS_MUTEX_H
    mtx_lock(&vp->v_interlock);
#else
    simple_lock(&vp->v_interlock);
#endif
    
    /* This node is on the freelist */
    if (vp->v_usecount <= 0) {
#if __FreeBSD__
	vm_object_t obj;

	obj = vp->v_object;

	if (obj != NULL
	    && (obj->ref_count != 0
#ifdef OBJ_MIGHTBEDIRTY
		|| (obj->flags & OBJ_MIGHTBEDIRTY) != 0
#endif
		)) {
#ifdef HAVE_SYS_MUTEX_H
	    mtx_unlock(&vp->v_interlock);
#else
	    simple_unlock (&vp->v_interlock);
#endif
	    return;
	}
#endif /* __FreeBSD__ */
	
	/*  DIAGNOSTIC */
	if (vp->v_usecount < 0 || vp->v_writecount != 0) {
		    vprint("vrele: bad ref count", vp);
		    panic("vrele: ref cnt");
	}
	
	NNPFSDEB(XDEBMSG, ("xfs_message_gc: success\n"));
	
#ifdef HAVE_KERNEL_VGONEL
	vgonel (vp, p);
#else
#ifdef HAVE_SYS_MUTEX_H
	mtx_unlock(&vp->v_interlock);
#else
	simple_unlock(&vp->v_interlock); 
#endif
	vgone (vp);
#endif

    } else {
#ifdef HAVE_SYS_MUTEX_H
	mtx_unlock(&vp->v_interlock);
#else
	simple_unlock(&vp->v_interlock);
#endif
	NNPFSDEB(XDEBMSG, ("xfs_message_gc: used\n"));
    }

}

#endif

int
xfs_message_gc_nodes(int fd,
		       struct xfs_message_gc_nodes *message,
		       u_int size,
		       d_thread_t *p)
{
    struct xfs_node *node;
    int i;

    NNPFSDEB(XDEBMSG, ("xfs_message_gc\n"));

    for (i = 0; i < message->len; i++) {
	node = xfs_node_find (&xfs[fd].nodehead, &message->handle[i]);
	if (node)
	    gc_vnode(XNODE_TO_VNODE(node), p);
	else {
	    NNPFSDEB(XDEBMSG, ("xfs_message_gc_nodes: no such node\n"));
	    send_inactive_node(fd, &message->handle[i]);
	}
    }

    return 0;
}


/*
 * Probe what version of xfs this support
 */

int
xfs_message_version(int fd,
		    struct xfs_message_version *message,
		    u_int size,
		    d_thread_t *p)
{
    struct xfs_message_wakeup msg;
    int ret;

    ret = NNPFS_VERSION;

    msg.header.opcode = NNPFS_MSG_WAKEUP;
    msg.sleepers_sequence_num = message->header.sequence_num;
    msg.error = ret;

    return xfs_message_send(fd, 
			      (struct xfs_message_header *) &msg, sizeof(msg));
}
