/*	$OpenBSD: xfs_node-bsd.c,v 1.2 2000/03/03 00:54:58 todd Exp $	*/

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

#include <xfs/xfs_locl.h>
#include <xfs/xfs_common.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_deb.h>

RCSID("$OpenBSD: xfs_node-bsd.c,v 1.2 2000/03/03 00:54:58 todd Exp $");

extern vop_t **xfs_vnodeop_p;

/*
 * Allocate a new vnode with handle `handle' in `mp' and return it in
 * `vpp'.  Return 0 or error.
 */

int
xfs_getnewvnode(struct mount *mp, struct vnode **vpp, 
		struct xfs_handle *handle)
{
    struct xfs_node *result;
    int error;

    error = getnewvnode(VT_AFS, mp, xfs_vnodeop_p, vpp);
    if (error)
	return error;
    
    result = xfs_alloc(sizeof(*result));
    bzero(result, sizeof(*result));
    
    (*vpp)->v_data = result;
    result->vn = *vpp;
    
    result->handle = *handle;
    result->flags = 0;
    result->tokens = 0;
    result->vnlocks = 0;

    result->anonrights = 0;
    
    return 0;
}

/*
 * Create a new xfs_node and make a vget
 *
 * Also prevents creation of duplicates. This happens
 * whenever there are more than one name to a file,
 * "." and ".." are common cases.  */

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
 again:
    /* Does not allow duplicates */
    result = xfs_node_find(xfsp, &node->handle);

    if (result == 0) {
	int error;
	struct vnode *v;

	error = xfs_getnewvnode(XFS_TO_VFS(xfsp), &v, &node->handle);
	if (error)
	    return error;

	result = VNODE_TO_XNODE(v);
	result->anonrights = node->anonrights;

	xfsp->nnodes++;
    } else {
	/* Node is already cached */
#ifdef HAVE_LK_INTERLOCK
	simple_lock(&(XNODE_TO_VNODE(result)->v_interlock));
#endif
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

    if (do_vget) {
	if (xfs_do_vget(XNODE_TO_VNODE(result), LK_INTERLOCK, p))
	    goto again;
    }

    *xpp = result;
    XFSDEB(XDEBNODE, ("return: new_xfs_node\n"));
    return 0;
}

void
free_xfs_node(struct xfs_node *node)
{
    struct xfs *xfsp = XFS_FROM_XNODE(node);

    XFSDEB(XDEBNODE, ("free_xfs_node starting: node = %p\n", node));

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
free_all_xfs_nodes(struct xfs *xfsp, int flags, int unmountp)
{
    int error = 0;
    struct mount *mp = XFS_TO_VFS(xfsp);

    if (mp == NULL) {
	XFSDEB(XDEBNODE, ("free_all_xfs_nodes already freed\n"));
	return 0;
    }

    XFSDEB(XDEBNODE, ("free_all_xfs_nodes starting\n"));

    xfs_dnlc_purge_mp(mp);

    if (xfsp->root) {
	XFSDEB(XDEBNODE, ("free_all_xfs_nodes now removing root\n"));

	vgone(XNODE_TO_VNODE(xfsp->root));
	xfsp->root = 0;
    }

    XFSDEB(XDEBNODE, ("free_all_xfs_nodes root removed\n"));
    XFSDEB(XDEBNODE, ("free_all_xfs_nodes now killing all remaining nodes\n"));

#ifdef HAVE_STRUCT_MOUNT_MNT_SYNCER
    if (!unmountp) {
	XFSDEB(XDEBNODE, ("free_all_xfs_nodes not flushing syncer vnode\n"));
	error = vflush(mp, mp->mnt_syncer, flags);
    } else
#endif
    {
	error = vflush(mp, NULL, flags);
    }

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

    for(t = XFS_TO_VFS(xfsp)->mnt_vnodelist.lh_first;
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
    if (va->va_mode != (mode_t)VNOVAL)
	XA_SET_MODE(xa, va->va_mode);
    if (va->va_nlink != VNOVAL)
	XA_SET_NLINK(xa, va->va_nlink);
    if (va->va_size != VNOVAL)
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

#define SET_TIMEVAL(X, S, N) do { (X)->tv_sec = (S); (X)->tv_nsec = (N); } while(0)

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
	SET_TIMEVAL(&va->va_atime, xa->xa_atime, 0);
    }
    if (XA_VALID_MTIME(xa)) {
	SET_TIMEVAL(&va->va_mtime, xa->xa_mtime, 0);
    }
    if (XA_VALID_CTIME(xa)) {
	SET_TIMEVAL(&va->va_ctime, xa->xa_ctime, 0);
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

/*
 * A single entry DNLC for systems for handling long names that don't
 * get put into the system DNLC.
 */

struct long_entry {
    struct vnode *dvp, *vp;
    char name[MAXNAMLEN + 1];
    size_t len;
    u_long dvpid, vpid;
};

static struct long_entry tbl;

/*
 * Nuke the `tbl'
 */

static void
tbl_clear (void)
{
    tbl.dvp = tbl.vp = NULL;
    tbl.name[0] = '\0';
    tbl.len = 0;
    tbl.dvpid = tbl.vpid = 0;
}

/*
 * Set the entry in the `tbl'
 */

static void
tbl_enter (size_t len, const char *name, struct vnode *dvp, struct vnode *vp)
{
    tbl.len = len;
    bcopy(name, tbl.name, len);
    tbl.dvp = dvp;
    tbl.vp = vp;
    tbl.dvpid = dvp->v_id;
    tbl.vpid = vp->v_id;
}

/*
 * Lookup in tbl
 */

static int
tbl_lookup (size_t len, const char *name, struct vnode *dvp, struct vnode **res)
{
    if (tbl.dvp == dvp
	&& tbl.len == len
	&& strncmp(tbl.name, name, len) == 0
	&& tbl.dvpid == tbl.dvp->v_id
	&& tbl.vpid == tbl.vp->v_id) {
	*res = tbl.vp;
	return -1;
    } else
	return 0;
}

/*
 * Store a componentname in the DNLC
 */

int
xfs_dnlc_enter(struct vnode *dvp,
	       xfs_componentname *cnp,
	       struct vnode *vp)
{
    XFSDEB(XDEBDNLC, ("xfs_dnlc_enter_cnp(%p, %p, %p)\n", dvp, cnp, vp));
    XFSDEB(XDEBDNLC, ("xfs_dnlc_enter: v_id = %ld\n", dvp->v_id));

    XFSDEB(XDEBDNLC, ("xfs_dnlc_enter: calling cache_enter:"
		      "dvp = %p, vp = %p, cnp = (%s, %ld, %lu), "
		      "nameiop = %lu, flags = %lx\n",
		      dvp, vp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_hash,
		      cnp->cn_nameiop, cnp->cn_flags));

#ifdef NCHNAMLEN
    if (cnp->cn_namelen <= NCHNAMLEN)
#endif
    {
	/*
	 * This is to make sure there's no negative entry already in the dnlc
	 */
	u_long save_nameiop;
	struct vnode *dummy;

	save_nameiop = cnp->cn_nameiop;
	cnp->cn_nameiop = CREATE;

	if (cache_lookup(dvp, &dummy, cnp) != 0) {
	    printf ("XFS PANIC WARNING! xfs_dnlc_enter: %s already in cache\n",
		    cnp->cn_nameptr);
	}

	cnp->cn_nameiop = save_nameiop;
	cache_enter(dvp, vp, cnp);
    }

    if (vp != NULL)
	tbl_enter (cnp->cn_namelen, cnp->cn_nameptr, dvp, vp);

    return 0;
}
		   

void
xfs_cnp_init (struct componentname *cn,
	      const char *name, struct vnode *vp, struct vnode *dvp,
	      struct proc *proc, struct ucred *cred,
	      int nameiop)
{
    char *p ;

    bzero(cn, sizeof(*cn));
    cn->cn_nameptr = (char *)name;
    cn->cn_namelen = strlen(name);
    cn->cn_flags   = 0;
    cn->cn_hash = 0;
    for (p = cn->cn_nameptr; *p; ++p)
	cn->cn_hash += *p;
    cn->cn_nameiop = nameiop;
    cn->cn_proc = proc;
    cn->cn_cred = cred;
}


/*
 * Store (dvp, name, vp) in the DNLC
 */

int
xfs_dnlc_enter_name(struct vnode *dvp,
		    const char *name,
		    struct vnode *vp)
{
    struct componentname cn;
    const char *p;

    XFSDEB(XDEBDNLC, ("xfs_dnlc_enter_name(%p, \"%s\", %p)\n", dvp, name, vp));

    cn.cn_nameiop = LOOKUP;
    cn.cn_flags   = 0;
    cn.cn_namelen = strlen(name);
    cn.cn_nameptr = (char *)name;
    cn.cn_hash = 0;
    for (p = cn.cn_nameptr; *p; ++p)
	cn.cn_hash += *p;

    return xfs_dnlc_enter (dvp, &cn, vp);
}

/*
 * Lookup (dvp, cnp) in the DNLC and return the result in `res'.
 * Return -1 if succesful, 0 if not and ENOENT if we're sure it doesn't exist.
 */

int
xfs_dnlc_lookup(struct vnode *dvp,
		xfs_componentname *cnp,
		struct vnode **res)
{
    int error;
    u_long saved_flags;

    XFSDEB(XDEBDNLC, ("xfs_dnlc_lookup(%p, \"%s\")\n", dvp, cnp->cn_nameptr));
    
    XFSDEB(XDEBDNLC, ("xfs_dnlc_lookup: v_id = %ld\n", dvp->v_id));
    
    XFSDEB(XDEBDNLC, ("xfs_dnlc_lookup: calling cache_lookup:"
		      "dvp = %p, cnp = (%s, %ld, %lu), flags = %lx\n",
		      dvp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_hash,
		      cnp->cn_flags));

    saved_flags = cnp->cn_flags;
    cnp->cn_flags |= MAKEENTRY;

    error = cache_lookup(dvp, res, cnp);

    cnp->cn_flags = saved_flags;

    XFSDEB(XDEBDNLC, ("xfs_dnlc_lookup: cache_lookup returned. "
		      "error = %d, *res = %p\n", error, *res));

    if (error == -1 || error == ENOENT)
	return error;

    return tbl_lookup (cnp->cn_namelen, cnp->cn_nameptr, dvp, res);
}

/*
 * Remove one entry from the DNLC
 */

void
xfs_dnlc_purge (struct vnode *vp)
{
    XFSDEB(XDEBDNLC, ("xfs_dnlc_purge\n"));

    if (tbl.dvp == vp || tbl.vp == vp)
	tbl_clear ();

    cache_purge(vp);
}

/*
 * Remove all entries belong to `mp' from the DNLC
 */

void
xfs_dnlc_purge_mp(struct mount *mp)
{
    XFSDEB(XDEBDNLC, ("xfs_dnlc_purge_mp()\n"));

    tbl_clear ();
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
