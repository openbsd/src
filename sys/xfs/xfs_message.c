/*	$OpenBSD: xfs_message.c,v 1.1 1998/08/30 16:47:21 art Exp $	*/
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

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/systm.h>

#include <xfs/xfs_common.h>
#include <xfs/xfs_deb.h>
#include <xfs/xfs_fs.h>
#include <sys/xfs_message.h>
#include <xfs/xfs_msg_locl.h>

RCSID("$KTH: xfs_message.c,v 1.16 1998/07/19 00:19:54 art Exp $");

int
xfs_message_installroot(int fd,
			struct xfs_message_installroot * message,
			u_int size,
			struct proc *p)
{
    int error = 0;

    XFSDEB(XDEBMSG, ("xfs_message_installroot\n"));

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

    XFSDEB(XDEBMSG, ("xfs_message_installnode\n"));

    dp = xfs_node_find(&xfs[fd], &message->parent_handle);
    if (dp) {
	error = new_xfs_node(&xfs[fd], &message->node, &n, p);
	if (error)
	    return error;
	cache_purge (XNODE_TO_VNODE(dp));
	xfs_dnlc_enter(XNODE_TO_VNODE(dp), message->name, XNODE_TO_VNODE(n));
	vrele(XNODE_TO_VNODE(n));
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

    t = xfs_node_find(&xfs[fd], &message->node.handle);
    if (t != 0) {
	t->tokens = message->node.tokens;
	xfs_attr2vattr(&message->node.attr, &t->attr);
	bcopy(message->node.id, t->id, sizeof(t->id));
	bcopy(message->node.rights, t->rights, sizeof(t->rights));
	t->anonrights = message->node.anonrights;
    } else {
	printf("XFS PANIC WARNING! xfs_message_installattr: no node!\n");
	error = ENOENT;
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

    XFSDEB(XDEBMSG, ("xfs_message_installdata\n"));

    t = xfs_node_find(&xfs[fd], &message->node.handle);

    if (t != NULL) {
	struct nameidata nd;
	char tmp[CACHEHANDLESIZE + 1];
	struct vnode *vp;

	bcopy((char *) &message->cache_handle.data, tmp, CACHEHANDLESIZE);
	tmp[CACHEHANDLESIZE] = '\0';

	XFSDEB(XDEBMSG, ("cache_handle = '%s'\n", tmp));

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, tmp, p);
	error = namei(&nd);
	vp = nd.ni_vp;

	if (error == 0) {
	    if (DATA_FROM_XNODE(t))
		vrele(DATA_FROM_XNODE(t));
	    VOP_UNLOCK(vp, 0, p);
	    DATA_FROM_XNODE(t) = vp;
	    XFSDEB(XDEBMSG, ("xfs_message_installdata: t = %p ; tokens = %x\n",
			     t, message->node.tokens));

	    t->tokens = message->node.tokens;
	    xfs_attr2vattr(&message->node.attr, &t->attr);
	    if (XNODE_TO_VNODE(t)->v_type == VDIR)
		cache_purge (XNODE_TO_VNODE(t));
	    bcopy(message->node.id, t->id, sizeof(t->id));
	    bcopy(message->node.rights, t->rights, sizeof(t->rights));
	    t->anonrights = message->node.anonrights;
	} else {
	    printf("XFS PANIC WARNING! xfs_message_installdata failed!\n");
	    printf("Reason: lookup failed on cache file '%s', error = %d\n",
		   tmp, error);
	}
    } else {
	printf("XFS PANIC WARNING! xfs_message_installdata failed\n");
	printf("Reason: No node to install the data into!\n");
	error = ENOENT;
    }

    return error;
}

int
xfs_message_invalidnode(int fd,
			struct xfs_message_invalidnode * message,
			u_int size,
			struct proc *p)
{
    int error = 0;
    struct xfs_node *t;

    XFSDEB(XDEBMSG, ("xfs_message_invalidnode\n"));

    t = xfs_node_find(&xfs[fd], &message->handle);
    if (t != 0) {
	/* XXX Really need to put back dirty data first. */
	if (DATA_FROM_XNODE(t)) {
	    vrele(DATA_FROM_XNODE(t));
	    DATA_FROM_XNODE(t) = (struct vnode *) 0;
	}
	XFS_TOKEN_CLEAR(t, ~0,
			XFS_OPEN_MASK | XFS_ATTR_MASK |
			XFS_DATA_MASK | XFS_LOCK_MASK);
	cache_purge(XNODE_TO_VNODE(t));
    } else {
	printf("XFS PANIC WARNING! xfs_message_invalidnode: no node!\n");
	error = ENOENT;
    }

    return error;
}
