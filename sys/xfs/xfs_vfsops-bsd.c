/*	$OpenBSD: xfs_vfsops-bsd.c,v 1.4 2000/03/03 00:54:59 todd Exp $	*/

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

#include <xfs/xfs_locl.h>

RCSID("$OpenBSD: xfs_vfsops-bsd.c,v 1.4 2000/03/03 00:54:59 todd Exp $");

/*
 * XFS vfs operations.
 */

#include <xfs/xfs_common.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_deb.h>
#include <xfs/xfs_vfsops.h>
#include <xfs/xfs_vfsops-bsd.h>
#include <xfs/xfs_vnodeops.h>

int
xfs_mount(struct mount *mp,
	  const char *user_path,
	  caddr_t user_data,
	  struct nameidata *ndp,
	  struct proc *p)
{
    return xfs_mount_common(mp, user_path, user_data, ndp, p);
}

int
xfs_start(struct mount * mp, int flags, struct proc * p)
{
    XFSDEB(XDEBVFOPS, ("xfs_start mp = %p, flags = %d, proc = %p\n", 
		       mp, flags, p));
    return 0;
}


int
xfs_unmount(struct mount * mp, int mntflags, struct proc *p)
{
    XFSDEB(XDEBVFOPS, ("xfs_umount: mp = %p, mntflags = %d, proc = %p\n", 
		       mp, mntflags, p));
    return xfs_unmount_common(mp, mntflags);
}

int
xfs_root(struct mount *mp, struct vnode **vpp)
{
    XFSDEB(XDEBVFOPS, ("xfs_root mp = %p\n", mp));
    return xfs_root_common(mp, vpp, curproc, curproc->p_ucred);
}

int
xfs_quotactl(struct mount *mp, int cmd, uid_t uid, caddr_t arg, struct proc *p)
{
    XFSDEB(XDEBVFOPS, ("xfs_quotactl: mp = %p, cmd = %d, uid = %u, "
		       "arg = %p, proc = %p\n", 
		       mp, cmd, uid, arg, p));
    return EOPNOTSUPP;
}

int
xfs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p)
{
    XFSDEB(XDEBVFOPS, ("xfs_statfs: mp = %p, sbp = %p, proc = %p\n", 
		       mp, sbp, p));
    bcopy(&mp->mnt_stat, sbp, sizeof(*sbp));
    return 0;
}

int
xfs_sync(struct mount *mp, int waitfor, struct ucred *cred, struct proc *p)
{
    XFSDEB(XDEBVFOPS, ("xfs_sync: mp = %p, waitfor = %d, "
		       "cred = %p, proc = %p\n",
		       mp, waitfor, cred, p));
    return 0;
}

int
xfs_vget(struct mount * mp,
	 ino_t ino,
	 struct vnode ** vpp)
{
    XFSDEB(XDEBVFOPS, ("xfs_vget\n"));
    return EOPNOTSUPP;
}

int
xfs_fhtovp(struct mount * mp,
	   struct fid * fhp,
	   struct vnode ** vpp)
{
#ifdef ARLA_KNFS
    static struct ucred fhtovpcred;
    struct netcred *np = NULL;
    struct xfs_node *xn;
    struct vnode *vp;
    xfs_handle handle;
    int error;

    XFSDEB(XDEBVFOPS, ("xfs_fhtovp\n"));

    if (fhp->fid_len != 16) {
	printf("xfs_fhtovp: *PANIC* got a invalid length of a fid\n");
	return EINVAL;
    }

    /* XXX: Should see if we is exported to this client */
#if 0
    np = vfs_export_lookup(mp, &ump->um_export, nam);
    if (np == NULL)
	return EACCES;
#endif

    memcpy(&handle, fhp->fid_data, sizeof(handle));
    XFSDEB(XDEBVFOPS, ("xfs_fhtovp: fid: %d.%d.%d.%d\n", 
		       handle.a, handle.d, handle.c, handle.d));

    XFSDEB(XDEBVFOPS, ("xfs_fhtovp: xfs_vnode_find\n"));
    xn = xfs_node_find(&xfs[0], &handle); /* XXX: 0 */

    if (xn == NULL) {
	struct xfs_message_getattr msg;

        error = xfs_getnewvnode(xfs[0].mp, &vp, &handle);
        if (error)
            return error;
	
	xfs_do_vget(vp, 0, current);

    } else {
	/* XXX access ? */
	vp = XNODE_TO_VNODE(xn);

	/* XXX wrong ? (we tell arla below) */
        if (vp->v_usecount <= 0) 
	    xfs_do_vget(vp, 0, current);
	else
	    VREF(vp);
	error = 0;
    }

    if (error == 0) {
	fhtovpcred.cr_uid = 0;
	fhtovpcred.cr_gid = 0;
	fhtovpcred.cr_ngroups = 0;
      
	*vpp = vp;

	XFSDEB(XDEBVFOPS, ("xfs_fhtovp done\n"));

	/* 
	 * XXX tell arla about this node is hold by nfsd.
	 * There need to be code in xfs_write too.
	 */
    } else
	XFSDEB(XDEBVFOPS, ("xfs_fhtovp failed (%d);", error));

    return error;
#else
    return EOPNOTSUPP;
#endif
}

int
xfs_vptofh(struct vnode * vp,
	   struct fid * fhp)
{
#ifdef ARLA_KNFS
    struct xfs_node *xn;
    XFSDEB(XDEBVFOPS, ("xfs_vptofh\n"));

    if (MAXFIDSZ < 16)
	return EOPNOTSUPP;

    xn = VNODE_TO_XNODE(vp);

    if (xn == NULL)
	return EINVAL;

    fhp->fid_len = 16;
    memcpy(fhp->fid_data, &xn->handle,  16);

    return 0;
#else
    XFSDEB(XDEBVFOPS, ("xfs_vptofh\n"));
    return EOPNOTSUPP;
#endif
}

/* 
 * xfs complete dead vnodes implementation.
 *
 * this is because the dead_vnodeops_p is _not_ filesystem, but rather
 * a part of the vfs-layer.  
 */

int
xfs_dead_lookup(struct vop_lookup_args * ap)
     /* struct vop_lookup_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
}; */
{
    *ap->a_vpp = NULL;
    return ENOTDIR;
}

/* 
 * Given `fsid', `fileid', and `gen', return in `vpp' a locked and
 * ref'ed vnode from that file system with that id and generation.
 * All is done in the context of `proc'.  Returns 0 if succesful, and
 * error otherwise.  
 */

int
xfs_fhlookup (struct proc *proc,
	      fsid_t fsid,
	      long fileid,
	      long gen,
	      struct vnode **vpp)
{
    int error;
    struct mount *mp;
    struct ucred *cred = proc->p_ucred;
    struct vattr vattr;

    XFSDEB(XDEBVFOPS, ("xfs_fhlookup: fileid = %ld\n",
		       fileid));

    error = suser (cred, NULL);
    if (error)
	return EPERM;

#ifdef HAVE_KERNEL_VFS_GETVFS
    mp = vfs_getvfs (&fsid);
#else
    mp = getvfs (&fsid);
#endif
    if (mp == NULL)
	return ENXIO;

    error = VFS_VGET(mp, fileid, vpp);

    if (error)
	return error;

    error = VOP_GETATTR(*vpp, &vattr, cred, proc);
    if (error) {
	vput(*vpp);
	return error;
    }

    if (vattr.va_gen != gen) {
	vput(*vpp);
	return ENOENT;
    }

#ifdef HAVE_KERNEL_VFS_OBJECT_CREATE
    if ((*vpp)->v_type == VREG && (*vpp)->v_object == NULL)
#if HAVE_FOUR_ARGUMENT_VFS_OBJECT_CREATE
	vfs_object_create (*vpp, proc, proc->p_ucred, TRUE);
#else
	vfs_object_create (*vpp, proc, proc->p_ucred);
#endif
#endif
    return 0;
}

/*
 * Perform an open operation on the vnode identified by (fsid, fileid,
 * gen) (see xfs_fhlookup) with flags `user_flags'.  Returns 0 or
 * error.  If succsesful, the file descriptor is returned in `retval'.
 */

int
xfs_fhopen (struct proc *proc,
	    fsid_t fsid,
	    long fileid,
	    long gen,
	    int user_flags,
	    register_t *retval)
{
    int error;
    struct vnode *vp;
    struct ucred *cred = proc->p_ucred;
    int flags = FFLAGS(user_flags);
    int index;
    struct file *fp;

    XFSDEB(XDEBVFOPS, ("xfs_fhopen: fileid = %ld, flags = %d\n",
		       fileid, user_flags));

    error = xfs_fhlookup (proc, fsid, fileid, gen, &vp);
    if (error)
	return error;

    error = VOP_OPEN(vp, flags, cred, proc);
    if (error)
	goto out;

    error = falloc(proc, &fp, &index);
    if (error)
	goto out;

    if (flags & FWRITE)
        vp->v_writecount++;

#if __FreeBSD_version >= 300000
    if (vp->v_type == VREG) {
#if HAVE_FOUR_ARGUMENT_VFS_OBJECT_CREATE
	error = vfs_object_create(vp, proc, proc->p_cred->pc_ucred, 1);
#else
	error = vfs_object_create(vp, proc, proc->p_cred->pc_ucred);
#endif
	if (error)
	    goto out;
    }
#endif

    fp->f_flag = flags & FMASK;
    fp->f_type = DTYPE_VNODE;
    fp->f_ops  = &vnops;
    fp->f_data = (caddr_t)vp;
    xfs_vfs_unlock(vp, proc);
    *retval = index;
    return 0;
out:
    vput(vp);
    return error;
}
