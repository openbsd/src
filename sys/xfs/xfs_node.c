/*	$OpenBSD: xfs_node.c,v 1.1 1998/08/30 16:47:21 art Exp $	*/
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
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/mount.h>

#include <xfs/xfs_common.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_deb.h>

RCSID("$KTH: xfs_node.c,v 1.16 1998/07/09 19:58:35 art Exp $");

#if defined(__NetBSD__) || defined(__OpenBSD__)
extern int (**xfs_vnodeop_p) (void *);
#elif defined(__FreeBSD__)
extern vop_t **xfs_vnodeop_p;
#endif

/*
 * Create a new xfs_node and make a VN_HOLD()!
 *
 * Also prevents creation of duplicates. This happens
 * whenever there are more than one name to a file,
 * "." and ".." are common cases.
 */

int
new_xfs_node(struct xfs *xfsp,
	     struct xfs_msg_node *node,
	     struct xfs_node **xpp,
	     struct proc *p)
{
    int do_vget = 0;
    struct xfs_node *result;

    XFSDEB(XDEBNODE, ("new_xfs_node %d.%d.%d.%d\n",
		      node->handle.a,
		      node->handle.b,
		      node->handle.c,
		      node->handle.d));

    /* Does not allow duplicates */
    result = xfs_node_find(xfsp, &node->handle);

    if (result == 0) {
	int error;
	struct vnode *v;

	result = xfs_alloc(sizeof(*result));
	if (result == 0) {
	    printf("xfs_alloc(%d) failed\n", (int) sizeof(*result));
	    panic("new_xfs_node: You Loose!");
	}
	bzero(result, sizeof(*result));

	error = getnewvnode(VT_AFS, XFS_TO_VFS(xfsp), xfs_vnodeop_p, &v);
	if (error) {
	    XFSDEB(XDEBVNOPS,
		   ("XFS PANIC! new_xfs_node: could not allocate node"));
	    return error;
	}
	v->v_data = result;
	result->vn = v;

	result->anonrights = node->anonrights;

	result->handle = node->handle;
	result->flags = 0;
	result->tokens = 0;

	xfsp->nnodes++;
    } else {
	/* Node is already cached */
	do_vget = 1;
    }

    /* Init other fields */
    xfs_attr2vattr(&node->attr, &result->attr);
    result->vn->v_type = result->attr.va_type;
    XFS_TOKEN_SET(result, XFS_ATTR_R, XFS_ATTR_MASK);
    bcopy(node->id, result->id, sizeof(result->id));
    bcopy(node->rights, result->rights, sizeof(result->rights));

    /*
     * We need to postpone this until here because (on FreeBSD) vget
     * tries to install a pager on the vnode and for that it wants to
     * retrieve the size with getattr.
     */

    if(do_vget)
	vget(XNODE_TO_VNODE(result), 0, p);

    *xpp = result;
    XFSDEB(XDEBNODE, ("return: new_xfs_node\n"));
    return 0;
}

void
free_xfs_node(struct xfs_node *node)
{
    struct xfs *xfsp = XFS_FROM_XNODE(node);

    XFSDEB(XDEBNODE, ("free_xfs_node starting\n"));

    /* XXX Really need to put back dirty data first. */

    if (DATA_FROM_XNODE(node)) {
	vrele(DATA_FROM_XNODE(node));
	DATA_FROM_XNODE(node) = NULL;
    }
    xfsp->nnodes--;
    XNODE_TO_VNODE(node)->v_data = NULL;
    xfs_free(node, sizeof(*node));

    XFSDEB(XDEBNODE, ("free_xfs_node done\n"));
}

int
free_all_xfs_nodes(struct xfs *xfsp, int flags)
{
    int error = 0;
    struct mount *mp = XFS_TO_VFS(xfsp);

    if (mp == NULL) {
	XFSDEB(XDEBNODE, ("free_all_xfs_nodes already freed\n"));
	return 0;
    }

    XFSDEB(XDEBNODE, ("free_all_xfs_nodes starting\n"));

    xfs_dnlc_purge(mp);

    XFSDEB(XDEBNODE, ("free_all_xfs_nodes now removing root\n"));

    if (xfsp->root) {
	vgone(XNODE_TO_VNODE(xfsp->root));
	xfsp->root = 0;
    }

    XFSDEB(XDEBNODE, ("free_all_xfs_nodes root removed\n"));
    XFSDEB(XDEBNODE, ("free_all_xfs_nodes now killing all remaining nodes\n"));

    error = vflush(mp, NULL, flags);

    if (error) {
	XFSDEB(XDEBNODE, ("xfree_all_xfs_nodes: vflush() error == %d\n",
			  error));
	return error;
    }

    XFSDEB(XDEBNODE, ("free_all_xfs_nodes done\n"));
    return error;
}

struct xfs_node *
xfs_node_find(struct xfs *xfsp, xfs_handle *handlep)
{
    struct vnode *t;
    struct xfs_node *xn = NULL;

    XFSDEB(XDEBNODE, ("xfs_node_find: xfsp = %p handlep = %p\n", 
		      xfsp, handlep));

    for (t = XFS_TO_VFS(xfsp)->mnt_vnodelist.lh_first;
	 t != NULL;
	 t = t->v_mntvnodes.le_next) {
	xn = VNODE_TO_XNODE(t);

	if (xn && xfs_handle_eq(&xn->handle, handlep))
	    break;
    }

    if (t != NULL)
	return xn;
    else
	return NULL;
}

void
vattr2xfs_attr(const struct vattr *va, struct xfs_attr *xa)
{
    bzero(xa, sizeof(*xa));
    if (va->va_mode != (mode_t) VNOVAL)
	XA_SET_MODE(xa, va->va_mode);
    if (va->va_nlink != VNOVAL)
	XA_SET_NLINK(xa, va->va_nlink);
    if (va->va_size != (u_quad_t) VNOVAL)
	XA_SET_SIZE(xa, va->va_size);
    if (va->va_uid != VNOVAL)
	XA_SET_UID(xa, va->va_uid);
    if (va->va_gid != VNOVAL)
	XA_SET_GID(xa, va->va_gid);
    if (va->va_atime.tv_sec != VNOVAL)
	XA_SET_ATIME(xa, va->va_atime.tv_sec);
    if (va->va_mtime.tv_sec != VNOVAL)
	XA_SET_MTIME(xa, va->va_mtime.tv_sec);
    if (va->va_ctime.tv_sec != VNOVAL)
	XA_SET_CTIME(xa, va->va_ctime.tv_sec);
    if (va->va_fileid != VNOVAL)
	XA_SET_FILEID(xa, va->va_fileid);
    switch (va->va_type) {
    case VNON:
	xa->xa_type = XFS_FILE_NON;
	break;
    case VREG:
	xa->xa_type = XFS_FILE_REG;
	break;
    case VDIR:
	xa->xa_type = XFS_FILE_DIR;
	break;
    case VBLK:
	xa->xa_type = XFS_FILE_BLK;
	break;
    case VCHR:
	xa->xa_type = XFS_FILE_CHR;
	break;
    case VLNK:
	xa->xa_type = XFS_FILE_LNK;
	break;
    case VSOCK:
	xa->xa_type = XFS_FILE_SOCK;
	break;
    case VFIFO:
	xa->xa_type = XFS_FILE_FIFO;
	break;
    case VBAD:
	xa->xa_type = XFS_FILE_BAD;
	break;
    default:
	panic("xfs_attr2attr: bad value");
    }
}

void
xfs_attr2vattr(const struct xfs_attr *xa, struct vattr *va)
{
    VATTR_NULL(va);
    if (XA_VALID_MODE(xa))
	va->va_mode = xa->xa_mode;
    if (XA_VALID_NLINK(xa))
	va->va_nlink = xa->xa_nlink;
    if (XA_VALID_SIZE(xa))
	va->va_size = xa->xa_size;
    if (XA_VALID_UID(xa))
	va->va_uid = xa->xa_uid;
    if (XA_VALID_GID(xa))
	va->va_gid = xa->xa_gid;
    if (XA_VALID_ATIME(xa)) {
	va->va_atime.tv_sec = xa->xa_atime;
	va->va_atime.tv_nsec = 0;
    }
    if (XA_VALID_MTIME(xa)) {
	va->va_mtime.tv_sec = xa->xa_mtime;
	va->va_mtime.tv_nsec = 0;
    }
    if (XA_VALID_CTIME(xa)) {
	va->va_ctime.tv_sec = xa->xa_ctime;
	va->va_ctime.tv_nsec = 0;
    }
    if (XA_VALID_FILEID(xa)) {
	va->va_fileid = xa->xa_fileid;
    }
    if (XA_VALID_TYPE(xa)) {
	switch (xa->xa_type) {
	case XFS_FILE_NON:
	    va->va_type = VNON;
	    break;
	case XFS_FILE_REG:
	    va->va_type = VREG;
	    break;
	case XFS_FILE_DIR:
	    va->va_type = VDIR;
	    break;
	case XFS_FILE_BLK:
	    va->va_type = VBLK;
	    break;
	case XFS_FILE_CHR:
	    va->va_type = VCHR;
	    break;
	case XFS_FILE_LNK:
	    va->va_type = VLNK;
	    break;
	case XFS_FILE_SOCK:
	    va->va_type = VSOCK;
	    break;
	case XFS_FILE_FIFO:
	    va->va_type = VFIFO;
	    break;
	case XFS_FILE_BAD:
	    va->va_type = VBAD;
	    break;
	default:
	    panic("xfs_attr2vattr: bad value");
	}
    }
    va->va_flags = 0;
    va->va_blocksize = 8192;
    va->va_bytes = va->va_size;
}


struct long_entry {
    struct vnode *dvp, *vp;
    char name[MAXNAMLEN + 1];
    size_t len;
    u_long dvpid, vpid;
};

static struct long_entry tbl;

int
xfs_dnlc_enter(struct vnode *dvp, char *name, struct vnode *vp)
{
    struct componentname cn;
    char *p;

    XFSDEB(XDEBDNLC, ("xfs_dnlc_enter(0x%x, \"%s\", 0x%x)\n",
		      (int) dvp, name, (int) vp));

    XFSDEB(XDEBDNLC, ("xfs_dnlc_enter: v_id = %ld\n",
		      dvp->v_id));

    cn.cn_namelen = strlen(name);
    cn.cn_nameptr = name;
    cn.cn_hash = 0;
    for (p = name; *p; ++p)
	cn.cn_hash += *p;

    XFSDEB(XDEBDNLC, ("xfs_dnlc_enter: calling cache_enter:"
		      "dvp = %p, vp = %p, cnp = (%s, %ld, %lu)\n",
		      dvp, vp, cn.cn_nameptr, cn.cn_namelen, cn.cn_hash));

    if (cn.cn_namelen <= NCHNAMLEN)
	cache_enter(dvp, vp, &cn);

    tbl.len = cn.cn_namelen;
    bcopy(name, tbl.name, tbl.len);
    tbl.dvp = dvp;
    tbl.vp = vp;
    tbl.dvpid = dvp->v_id;
    tbl.vpid = vp->v_id;

    return 0;
}

int
xfs_dnlc_lookup(struct vnode *dvp,
		struct componentname *cnp,
		struct vnode **res)
{
    int error;

    XFSDEB(XDEBDNLC, ("xfs_dnlc_lookup(0x%x, \"%s\")\n",
		      (int) dvp, cnp->cn_nameptr));

    XFSDEB(XDEBDNLC, ("xfs_dnlc_lookup: v_id = %ld\n",
		      dvp->v_id));

    XFSDEB(XDEBDNLC, ("xfs_dnlc_lookup: calling cache_lookup:"
		      "dvp = %p, cnp = (%s, %ld, %lu), flags = %lx\n",
		      dvp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_hash,
		      cnp->cn_flags));

    cnp->cn_flags |= MAKEENTRY;

    error = cache_lookup(dvp, res, cnp);

    XFSDEB(XDEBDNLC, ("xfs_dnlc_lookup: cache_lookup returned. "
		      "error == %d, *res == %p\n", error, *res));

    if (error == -1 || error == ENOENT)
	return error;

    if (tbl.dvp == dvp
	&& tbl.len == cnp->cn_namelen
	&& strncmp(tbl.name, cnp->cn_nameptr, cnp->cn_namelen) == 0
	&& tbl.dvpid == tbl.dvp->v_id
	&& tbl.vpid == tbl.vp->v_id) {
	*res = tbl.vp;
	return -1;
    }

    return 0;
}

void
xfs_dnlc_purge(struct mount *mp)
{
    XFSDEB(XDEBDNLC, ("xfs_dnlc_purge()\n"));

    tbl.dvp = tbl.vp = NULL;
    tbl.name[0] = '\0';
    tbl.len = 0;
    tbl.dvpid = tbl.vpid = 0;

    cache_purgevfs(mp);
}

/*
 * Returns 1 if pag has any rights set in the node
 */
int
xfs_has_pag(const struct xfs_node *xn, pag_t pag)
{
    int i;

    for (i = 0; i < MAXRIGHTS; i++)
	if (xn->id[i] == pag)
	    return 1;

    return 0;
}
