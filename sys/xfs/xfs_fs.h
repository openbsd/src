/*	$OpenBSD: xfs_fs.h,v 1.1 1998/08/30 16:47:21 art Exp $	*/
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

/* $KTH: xfs_fs.h,v 1.8 1998/04/04 01:17:26 art Exp $ */

#ifndef _xfs_h
#define _xfs_h

#include <sys/types.h>

#include <xfs/xfs_node.h>
#include <sys/xfs_attr.h>

#define NXFS 2 /* maximal number of filesystems on a single device */

/*
 * Filesystem struct.
 */
struct xfs {
    u_int status;		       /* Inited, opened or mounted */
#define XFS_MOUNTED	0x1
    struct mount *mp;
    struct xfs_node *root;
    u_int nnodes;

    int fd;
};

#define VFS_TO_XFS(v)      ((struct xfs *) ((v)->mnt_data))
#define XFS_TO_VFS(x)      ((x)->mp)

#define XFS_FROM_VNODE(vp) VFS_TO_XFS((vp)->v_mount)
#define XFS_FROM_XNODE(xp) XFS_FROM_VNODE(XNODE_TO_VNODE(xp))

extern struct xfs xfs[];

extern struct vnodeops xfs_vnodeops;

struct xfs_node *xfs_node_find(struct xfs *, struct xfs_handle *);
int new_xfs_node(struct xfs *, struct xfs_msg_node *, struct xfs_node **,
		 struct proc *);
void free_xfs_node(struct xfs_node *);
int free_all_xfs_nodes(struct xfs *, int);

int xfs_dnlc_enter(struct vnode *, char *, struct vnode *);
void xfs_dnlc_purge(struct mount *);
int xfs_dnlc_lookup(struct vnode *, struct componentname *, struct vnode **);

void vattr2xfs_attr(const struct vattr * va, struct xfs_attr *xa);
void xfs_attr2vattr(const struct xfs_attr *xa, struct vattr * va);

int xfs_has_pag(const struct xfs_node *xn, pag_t pag);

#endif				       /* _xfs_h */
