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

RCSID("$Id: xfs_message.c,v 1.8 2002/06/07 04:10:32 hin Exp $");

int
xfs_message_installroot(int fd,
			struct xfs_message_installroot * message,
			u_int size,
			struct proc *p)
{
    int error = 0;

    XFSDEB(XDEBMSG, ("xfs_message_installroot (%d,%d,%d,%d)\n",
		     message->node.handle.a,
		     message->node.handle.b,
		     message->node.handle.c,
		     message->node.handle.d));

    if (xfs[fd].root != NULL) {
	printf("XFS PANIC WARNING! xfs_message_installroot: called again!\n");
	error = EBUSY;
    } else {
	error = new_xfs_node(&xfs[fd], &message->node, &xfs[fd].root, p);
	if (error)
	    return error;
	xfs[fd].root->vn->v_flag |= VROOT;
    }
    return error;
}

int
xfs_message_installnode(int fd,
			struct xfs_message_installnode * message,
			u_int size,
			struct proc *p)
{
    int error = 0;
    struct xfs_node *n, *dp;

    XFSDEB(XDEBMSG, ("xfs_message_installnode (%d,%d,%d,%d)\n",
		     message->node.handle.a,
		     message->node.handle.b,
		     message->node.handle.c,
		     message->node.handle.d));

retry:
    dp = xfs_node_find(&xfs[fd], &message->parent_handle);
    if (dp) {
	struct vnode *t_vnode = XNODE_TO_VNODE(dp);

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
	printf("XFS PANIC WARNING! xfs_message_installnode: no parent\n");
	error = ENOENT;
    }
    XFSDEB(XDEBMSG, ("return: xfs_message_installnode: %d\n", error));

    return error;
}

int
xfs_message_installattr(int fd,
			struct xfs_message_installattr * message,
			u_int size,
			struct proc *p)
{
    int error = 0;
    struct xfs_node *t;

    XFSDEB(XDEBMSG, ("xfs_message_installattr (%d,%d,%d,%d) \n",
		     message->node.handle.a,
		     message->node.handle.b,
		     message->node.handle.c,
		     message->node.handle.d));

    t = xfs_node_find(&xfs[fd], &message->node.handle);
    if (t != 0) {
	t->tokens = message->node.tokens;
	if ((t->tokens & XFS_DATA_MASK) && DATA_FROM_XNODE(t) == NULL) {
	    printf ("xfs_message_installattr: tokens and no data\n");
	    t->tokens &= ~XFS_DATA_MASK;
	}
	xfs_attr2vattr(&message->node.attr, &t->attr, 0);
	xfs_set_vp_size(XNODE_TO_VNODE(t), t->attr.va_size);
	bcopy(message->node.id, t->id, sizeof(t->id));
	bcopy(message->node.rights, t->rights, sizeof(t->rights));
	t->anonrights = message->node.anonrights;
    } else {
	XFSDEB(XDEBMSG, ("xfs_message_installattr: no such node\n"));
    }
    
    return error;
}

int
xfs_message_installdata(int fd,
			struct xfs_message_installdata * message,
			u_int size,
			struct proc *p)
{
    struct xfs_node *t;
    int error = 0;

    XFSDEB(XDEBMSG, ("xfs_message_installdata (%d,%d,%d,%d)\n",
		     message->node.handle.a,
		     message->node.handle.b,
		     message->node.handle.c,
		     message->node.handle.d));

retry:
    t = xfs_node_find(&xfs[fd], &message->node.handle);
    if (t != NULL) {
	struct xfs_fhandle_t *fh = (struct xfs_fhandle_t *)&message->cache_handle;
	struct vnode *vp;
	struct vnode *t_vnode = XNODE_TO_VNODE(t);

	message->cache_name[sizeof(message->cache_name)-1] = '\0';
	XFSDEB(XDEBMSG, ("cache_name = '%s'\n", message->cache_name));

	if (xfs_do_vget(t_vnode, 0 /* LK_SHARED */, p))
		goto retry;

	if (message->flag & XFS_ID_HANDLE_VALID) {
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

	    XFSDEB(XDEBMSG,
		   ("xfs_message_installdata: fhlookup failed: %d, "
		    "opening by name\n", error));

	    NDINIT(ndp, LOOKUP, FOLLOW, UIO_SYSSPACE,
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

	    XFSDEB(XDEBMSG, ("xfs_message_installdata: t = %lx;"
			     " tokens = %x\n",
			     (unsigned long)t, message->node.tokens));

	    t->tokens = message->node.tokens;
	    xfs_attr2vattr(&message->node.attr, &t->attr, 1);
	    xfs_set_vp_size(XNODE_TO_VNODE(t), t->attr.va_size);
	    if (XNODE_TO_VNODE(t)->v_type == VDIR
		&& (message->flag & XFS_ID_INVALID_DNLC))
		cache_purge (XNODE_TO_VNODE(t));
	    bcopy(message->node.id, t->id, sizeof(t->id));
	    bcopy(message->node.rights, t->rights, sizeof(t->rights));
	    t->anonrights = message->node.anonrights;
#if 0
	    if (message->flag & XFS_ID_AFSDIR)
		t->flags |= XFS_AFSDIR;
#endif
	} else {
	    printf("XFS PANIC WARNING! xfs_message_installdata failed!\n");
	    printf("Reason: lookup failed on cache file '%s', error = %d\n",
		   message->cache_name, error);
	}
	vrele (t_vnode);
    } else {
	printf("XFS PANIC WARNING! xfs_message_installdata failed\n");
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
			struct proc *p)
{
    int error = 0;
    struct xfs_node *t;

    XFSDEB(XDEBMSG, ("xfs_message_invalidnode (%d,%d,%d,%d)\n",
		     message->handle.a,
		     message->handle.b,
		     message->handle.c,
		     message->handle.d));

    t = xfs_node_find(&xfs[fd], &message->handle);
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
	    if (UBCISVALID(vp) && !ubc_isinuse(vp, 0)) {
		ubc_setsize(vp, 0);
	    } else {
		t->flags |= XFS_STALE;
		return 0;
	    }
#else
	    t->flags |= XFS_STALE;
	    return 0;
#endif
	}

	if (DATA_FROM_XNODE(t)) {
	    vrele(DATA_FROM_XNODE(t));
	    DATA_FROM_XNODE(t) = (struct vnode *) 0;
	}
	XFS_TOKEN_CLEAR(t, ~0,
			XFS_OPEN_MASK | XFS_ATTR_MASK |
			XFS_DATA_MASK | XFS_LOCK_MASK);
	/* Dir changed, must invalidate DNLC. */
	if (vp->v_type == VDIR)
	    xfs_dnlc_purge(vp);
	if (vp->v_usecount == 0) {
	    XFSDEB(XDEBVNOPS, ("xfs_message_invalidnode: vrecycle\n"));
#ifndef __osf__
	    vrecycle(vp, 0, p);
#endif
	}
    } else {
	error = ENOENT;
    }

    return error;
}

int
xfs_message_updatefid(int fd,
		      struct xfs_message_updatefid * message,
		      u_int size,
		      struct proc *p)
{
    int error = 0;
    struct xfs_node *t;

    XFSDEB(XDEBMSG, ("xfs_message_updatefid (%d,%d,%d,%d)\n",
		     message->old_handle.a,
		     message->old_handle.b,
		     message->old_handle.c,
		     message->old_handle.d));

    t = xfs_node_find (&xfs[fd], &message->old_handle);
    if (t != NULL) {
	t->handle = message->new_handle;
    } else {
	printf ("XFS PANIC WARNING! xfs_message_updatefid: no node!\n");
	error = ENOENT;
    }
    return error;
}

#if __osf__

/*
 * Try to clean out nodes for the userland daemon
 */

static void
gc_vnode (struct vnode *vp,
	  struct proc *p)
{
    /* This node is on the freelist */
    if (vp->v_usecount <= 0) {
	
	/*  DIAGNOSTIC */
	if (vp->v_usecount < 0) {
		    vprint("vrele: bad ref count", vp);
		    panic("vrele: ref cnt");
	}
	
	XFSDEB(XDEBMSG, ("xfs_message_gc: success\n"));
	
	vgone(vp, VX_NOSLEEP, NULL);
    } else {
	XFSDEB(XDEBMSG, ("xfs_message_gc: used\n"));
    }

}

int
xfs_message_gc_nodes(int fd,
		     struct xfs_message_gc_nodes *message,
		     u_int size,
		     struct proc *p)
{
    XFSDEB(XDEBMSG, ("xfs_message_gc\n"));

    if (message->len == 0) {
	struct vnode *vp;

	/* XXX see comment in xfs_node_find */

	for(vp = XFS_TO_VFS(&xfs[fd])->m_mounth;
	    vp != NULL; 
	    vp = vp->v_mountf) {
	    gc_vnode (vp, p);
	}

    } else {
	struct xfs_node *t;
	int i;

	for (i = 0; i < message->len; i++) {
	    t = xfs_node_find (&xfs[fd], &message->handle[i]);
	    if (t == NULL)
		continue;

	    gc_vnode(XNODE_TO_VNODE(t), p);
	}
    }

    return 0;
}

#else /* !__osf__ */

/*
 * Try to clean out nodes for the userland daemon
 */

static void
gc_vnode (struct vnode *vp,
	  struct proc *p)
{
    simple_lock(&vp->v_interlock);
    
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
	    simple_unlock (&vp->v_interlock);
	    return;
	}
#endif /* __FreeBSD__ */
	
	/*  DIAGNOSTIC */
	if (vp->v_usecount < 0 || vp->v_writecount != 0) {
		    vprint("Pjäxomatic-4700: bad ref count", vp);
		    panic("Pjäxomatic-4650: ref cnt");
	}
	
	XFSDEB(XDEBMSG, ("xfs_message_gc: success\n"));
	
#ifdef HAVE_KERNEL_FUNC_VGONEL
	vgonel (vp, p);
#else
	simple_unlock(&vp->v_interlock); 
	vgone (vp);
#endif

    } else {
	simple_unlock(&vp->v_interlock);
	XFSDEB(XDEBMSG, ("xfs_message_gc: used\n"));
    }

}

int
xfs_message_gc_nodes(int fd,
		     struct xfs_message_gc_nodes *message,
		     u_int size,
		     struct proc *p)
{
    XFSDEB(XDEBMSG, ("xfs_message_gc\n"));

    if (message->len == 0) {
	struct vnode *vp, *next;

	/* XXX see comment in xfs_node_find */
	/* XXXSMP do gone[l] need to get mntvnode_slock ? */

/* FreeBSD 4.5 and above did rename mnt_vnodelist to mnt_nvnodelist */
#ifdef HAVE_STRUCT_MOUNT_MNT_NVNODELIST
	for(vp = TAILQ_FIRST(&XFS_TO_VFS(&xfs[fd])->mnt_nvnodelist);
	    vp != NULL; 
	    vp = next) {

	    next = TAILQ_NEXT(vp, v_nmntvnodes);
	    gc_vnode (vp, p);
	}
#else
	for(vp = XFS_TO_VFS(&xfs[fd])->mnt_vnodelist.lh_first;
	    vp != NULL; 
	    vp = next) {

	    next = vp->v_mntvnodes.le_next;
	    gc_vnode (vp, p);
	}
#endif
    } else {
	struct xfs_node *t;
	int i;

	for (i = 0; i < message->len; i++) {
	    t = xfs_node_find (&xfs[fd], &message->handle[i]);
	    if (t == NULL)
		continue;

	    gc_vnode(XNODE_TO_VNODE(t), p);
	}
    }

    return 0;
}


#endif

/*
 * Probe what version of xfs this support
 */

int
xfs_message_version(int fd,
		    struct xfs_message_version *message,
		    u_int size,
		    struct proc *p)
{
    struct xfs_message_wakeup msg;
    int ret;

    ret = XFS_VERSION;

    msg.header.opcode = XFS_MSG_WAKEUP;
    msg.sleepers_sequence_num = message->header.sequence_num;
    msg.error = ret;

    return xfs_message_send(fd, (struct xfs_message_header *) &msg, sizeof(msg));
}
