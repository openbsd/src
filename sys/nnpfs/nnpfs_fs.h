/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

/* $arla: nnpfs_fs.h,v 1.22 2002/12/19 09:49:19 lha Exp $ */

#ifndef _nnpfs_h
#define _nnpfs_h

#include <sys/types.h>

#include <nnpfs/nnpfs_common.h>
#include <nnpfs/nnpfs_node.h>
#include <nnpfs/nnpfs_attr.h>

#define NNNPFS 2 /* maximal number of filesystems on a single device */

/*
 * Filesystem struct.
 */

struct nnpfs {
    u_int status;		       /* Inited, opened or mounted */
#define NNPFS_MOUNTED	0x1
    struct mount *mp;
    struct nnpfs_node *root;
    u_int nnodes;
    int fd;
    struct nnpfs_nodelist_head nodehead;
};

#ifdef __osf__
#ifdef HAVE_STRUCT_MOUNT_M_INFO
#define VFS_TO_NNPFS(v)      ((struct nnpfs *) ((v)->m_info))
#define VFS_ASSIGN(v, val)   do { (v)->m_info = (void *) (val); } while (0)
#else
#define VFS_TO_NNPFS(v)      ((struct nnpfs *) ((v)->m_data))
#define VFS_ASSIGN(v, val)   do { (v)->m_data = (void *) (val); } while (0)
#endif
#else
#define VFS_TO_NNPFS(v)      ((struct nnpfs *) ((v)->mnt_data))
#define VFS_ASSIGN(v, val)   do { (v)->mnt_data = (void *) (val); } while (0)
#endif
#define NNPFS_TO_VFS(x)      ((x)->mp)

#define NNPFS_FROM_VNODE(vp) VFS_TO_NNPFS((vp)->v_mount)
#define NNPFS_FROM_XNODE(xp) NNPFS_FROM_VNODE(XNODE_TO_VNODE(xp))

extern struct nnpfs nnpfs[];

extern struct vnodeops nnpfs_vnodeops;

int new_nnpfs_node(struct nnpfs *, struct nnpfs_msg_node *, struct nnpfs_node **,
		 d_thread_t *);
void free_nnpfs_node(struct nnpfs_node *);
int free_all_nnpfs_nodes(struct nnpfs *, int, int);

int nnpfs_dnlc_enter(struct vnode *, nnpfs_componentname *, struct vnode *);
int nnpfs_dnlc_enter_name(struct vnode *, const char *, struct vnode *);
void nnpfs_dnlc_purge_mp(struct mount *);
void nnpfs_dnlc_purge(struct vnode *);
int nnpfs_dnlc_lookup(struct vnode *, nnpfs_componentname *, struct vnode **);
int nnpfs_dnlc_lookup_name(struct vnode *, const char *, struct vnode **);

void vattr2nnpfs_attr(const struct vattr *, struct nnpfs_attr *);
void nnpfs_attr2vattr(const struct nnpfs_attr *, struct vattr *, int);

int nnpfs_has_pag(const struct nnpfs_node *, nnpfs_pag_t);

#endif				       /* _nnpfs_h */
