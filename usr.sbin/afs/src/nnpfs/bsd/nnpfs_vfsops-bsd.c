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

#include <nnpfs/nnpfs_locl.h>

RCSID("$arla: nnpfs_vfsops-bsd.c,v 1.72 2002/12/19 10:30:17 lha Exp $");

/*
 * NNPFS vfs operations.
 */

#include <nnpfs/nnpfs_common.h>
#include <nnpfs/nnpfs_message.h>
#include <nnpfs/nnpfs_fs.h>
#include <nnpfs/nnpfs_dev.h>
#include <nnpfs/nnpfs_deb.h>
#include <nnpfs/nnpfs_vfsops.h>
#include <nnpfs/nnpfs_vfsops-bsd.h>
#include <nnpfs/nnpfs_vnodeops.h>

int
nnpfs_mount_caddr(struct mount *mp,
		const char *user_path,
		caddr_t user_data,
		struct nameidata *ndp,
		d_thread_t *p)
{
    return nnpfs_mount_common(mp, user_path, user_data, ndp, p);
}

int
nnpfs_start(struct mount * mp, int flags, d_thread_t * p)
{
    NNPFSDEB(XDEBVFOPS, ("nnpfs_start mp = %lx, flags = %d, proc = %lx\n", 
		       (unsigned long)mp, flags, (unsigned long)p));
    return 0;
}


int
nnpfs_unmount(struct mount * mp, int mntflags, d_thread_t *p)
{
    NNPFSDEB(XDEBVFOPS, ("nnpfs_umount: mp = %lx, mntflags = %d, proc = %lx\n", 
		       (unsigned long)mp, mntflags, (unsigned long)p));
    return nnpfs_unmount_common(mp, mntflags);
}

int
nnpfs_root(struct mount *mp, struct vnode **vpp)
{
    NNPFSDEB(XDEBVFOPS, ("nnpfs_root mp = %lx\n", (unsigned long)mp));
#ifdef HAVE_FREEBSD_THREAD
    return nnpfs_root_common(mp, vpp, nnpfs_curthread(), nnpfs_curthread()->td_proc->p_ucred);
#else
    return nnpfs_root_common(mp, vpp, nnpfs_curproc(), nnpfs_curproc()->p_ucred);
#endif
}

int
nnpfs_quotactl(struct mount *mp, int cmd, uid_t uid, caddr_t arg, d_thread_t *p)
{
    NNPFSDEB(XDEBVFOPS, ("nnpfs_quotactl: mp = %lx, cmd = %d, uid = %u, "
		       "arg = %lx, proc = %lx\n", 
		       (unsigned long)mp, cmd, uid,
		       (unsigned long)arg, (unsigned long)p));
    return EOPNOTSUPP;
}

int
nnpfs_statfs(struct mount *mp, struct statfs *sbp, d_thread_t *p)
{
    NNPFSDEB(XDEBVFOPS, ("nnpfs_statfs: mp = %lx, sbp = %lx, proc = %lx\n", 
		       (unsigned long)mp,
		       (unsigned long)sbp,
		       (unsigned long)p));
    bcopy(&mp->mnt_stat, sbp, sizeof(*sbp));
    return 0;
}

int
nnpfs_sync(struct mount *mp, int waitfor, struct ucred *cred, d_thread_t *p)
{
    NNPFSDEB(XDEBVFOPS, ("nnpfs_sync: mp = %lx, waitfor = %d, "
		       "cred = %lx, proc = %lx\n",
		       (unsigned long)mp,
		       waitfor,
		       (unsigned long)cred,
		       (unsigned long)p));
    return 0;
}

int
nnpfs_vget(struct mount * mp,
#ifdef __APPLE__
	 void *ino,
#else
	 ino_t ino,
#endif
	 struct vnode ** vpp)
{
    NNPFSDEB(XDEBVFOPS, ("nnpfs_vget\n"));
    return EOPNOTSUPP;
}

static int
common_fhtovp(struct mount * mp,
	   struct fid * fhp,
	   struct vnode ** vpp)
{
#ifdef ARLA_KNFS
    struct netcred *np = NULL;
    struct nnpfs_node *xn;
    struct vnode *vp;
    nnpfs_handle handle;
    int error;

    NNPFSDEB(XDEBVFOPS, ("nnpfs_fhtovp\n"));

    if (fhp->fid_len != 16) {
	printf("nnpfs_fhtovp: *PANIC* got a invalid length of a fid\n");
	return EINVAL;
    }

    memcpy(&handle, fhp->fid_data, sizeof(handle));
    NNPFSDEB(XDEBVFOPS, ("nnpfs_fhtovp: fid: %d.%d.%d.%d\n", 
		       handle.a, handle.d, handle.c, handle.d));

    NNPFSDEB(XDEBVFOPS, ("nnpfs_fhtovp: nnpfs_vnode_find\n"));
    xn = nnpfs_node_find(&nnpfs[0].nodehead, &handle);

    if (xn == NULL) {
	struct nnpfs_message_getattr msg;

        error = nnpfs_getnewvnode(nnpfs[0].mp, &vp, &handle);
        if (error)
            return error;
	
	nnpfs_do_vget(vp, 0, curproc);

    } else {
	/* XXX access ? */
	vp = XNODE_TO_VNODE(xn);

	/* XXX wrong ? (we tell arla below) */
        if (vp->v_usecount <= 0) 
	    nnpfs_do_vget(vp, 0, curproc);
	else
	    VREF(vp);
	error = 0;
    }

    *vpp = vp;

    if (error == 0) {
	NNPFSDEB(XDEBVFOPS, ("nnpfs_fhtovp done\n"));

	/* 
	 * XXX tell arla about this node is hold by nfsd.
	 * There need to be code in nnpfs_write too.
	 */
    } else
	NNPFSDEB(XDEBVFOPS, ("nnpfs_fhtovp failed (%d)\n", error));

    return error;
#else /* !ARLA_KNFS */
    return EOPNOTSUPP;
#endif /* !ARLA_KNFS */
}

/* new style fhtovp */

#ifdef HAVE_STRUCT_VFSOPS_VFS_CHECKEXP
int
nnpfs_fhtovp(struct mount * mp,
	   struct fid * fhp,
	   struct vnode ** vpp)
{
    return common_fhtovp (mp, fhp, vpp);
}

#else /* !HAVE_STRUCT_VFSOPS_VFS_CHECKEXP */

/* old style fhtovp */

int
nnpfs_fhtovp(struct mount * mp,
	   struct fid * fhp,
	   struct mbuf * nam,
	   struct vnode ** vpp,
	   int *exflagsp,
	   struct ucred ** credanonp)
{
    static struct ucred fhtovpcred;
    int error;

    /* XXX: Should see if we is exported to this client */
#if 0
    np = vfs_export_lookup(mp, &ump->um_export, nam);
    if (np == NULL)
       return EACCES;
#endif
    error = common_fhtovp(mp, fhp, vpp);
    if (error == 0) {
       fhtovpcred.cr_uid = 0;
       fhtovpcred.cr_gid = 0;
       fhtovpcred.cr_ngroups = 0;
      
#ifdef MNT_EXPUBLIC
       *exflagsp = MNT_EXPUBLIC;
#else
       *exflagsp = 0;
#endif
       *credanonp = &fhtovpcred;
    }
    return error;
}
#endif /* !HAVE_STRUCT_VFSOPS_VFS_CHECKEXP */

int
nnpfs_checkexp (struct mount *mp,
#ifdef __FreeBSD__
	      struct sockaddr *nam,
#else
	      struct mbuf *nam,
#endif
	      int *exflagsp,
	      struct ucred **credanonp)
{
    struct netcred *np;

    NNPFSDEB(XDEBVFOPS, ("nnpfs_checkexp\n"));

#if 0
    np = vfs_export_lookup(mp, &ump->um_export, nam);
    if (np == NULL)
	return EACCES;
#endif
    return 0;
}

int
nnpfs_vptofh(struct vnode * vp,
	   struct fid * fhp)
{
#ifdef ARLA_KNFS
    struct nnpfs_node *xn;
    NNPFSDEB(XDEBVFOPS, ("nnpfs_vptofh\n"));

    if (MAXFIDSZ < 16)
	return EOPNOTSUPP;

    xn = VNODE_TO_XNODE(vp);

    if (xn == NULL)
	return EINVAL;

    fhp->fid_len = 16;
    memcpy(fhp->fid_data, &xn->handle,  16);

    return 0;
#else
    NNPFSDEB(XDEBVFOPS, ("nnpfs_vptofh\n"));
    return EOPNOTSUPP;
#endif
}

/* 
 * nnpfs complete dead vnodes implementation.
 *
 * this is because the dead_vnodeops_p is _not_ filesystem, but rather
 * a part of the vfs-layer.  
 */

int
nnpfs_dead_lookup(struct vop_lookup_args * ap)
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
nnpfs_fhlookup (d_thread_t *proc,
	      struct nnpfs_fhandle_t *fhp,
	      struct vnode **vpp)
{
    int error;
    struct mount *mp;
#if !(defined(HAVE_GETFH) && defined(HAVE_FHOPEN))
    struct ucred *cred = proc->p_ucred;
    struct vattr vattr;
    fsid_t fsid;
    struct nnpfs_fh_args *fh_args = (struct nnpfs_fh_args *)fhp->fhdata;

    NNPFSDEB(XDEBVFOPS, ("nnpfs_fhlookup (nnpfs)\n"));

    error = nnpfs_suser (proc);
    if (error)
	return EPERM;

    if (fhp->len < sizeof(struct nnpfs_fh_args))
	return EINVAL;
    
    fsid = SCARG(fh_args, fsid);

    mp = nnpfs_vfs_getvfs (&fsid);
    if (mp == NULL)
	return ENXIO;

#ifdef __APPLE__
    {
	uint32_t ino = SCARG(fh_args, fileid);
	error = VFS_VGET(mp, &ino, vpp);
    }
#else
    error = VFS_VGET(mp, SCARG(fh_args, fileid), vpp);
#endif

    if (error)
	return error;

    if (*vpp == NULL)
	return ENOENT;

    error = VOP_GETATTR(*vpp, &vattr, cred, proc);
    if (error) {
	vput(*vpp);
	return error;
    }

    if (vattr.va_gen != SCARG(fh_args, gen)) {
	vput(*vpp);
	return ENOENT;
    }
#else /* HAVE_GETFH && HAVE_FHOPEN */
    {
	fhandle_t *fh = (fhandle_t *) fhp;

	NNPFSDEB(XDEBVFOPS, ("nnpfs_fhlookup (native)\n"));

	mp = nnpfs_vfs_getvfs (&fh->fh_fsid);
	if (mp == NULL)
	    return ESTALE;

	if ((error = VFS_FHTOVP(mp, &fh->fh_fid, vpp)) != 0) {
	    *vpp = NULL;
	    return error;
	}
    }
#endif  /* HAVE_GETFH && HAVE_FHOPEN */

#ifdef HAVE_KERNEL_VFS_OBJECT_CREATE
    if ((*vpp)->v_type == VREG && (*vpp)->v_object == NULL)
#ifdef HAVE_FREEBSD_THREAD
	nnpfs_vfs_object_create (*vpp, proc, proc->td_proc->p_ucred);
#else
	nnpfs_vfs_object_create (*vpp, proc, proc->p_ucred);
#endif
#elif __APPLE__
    if ((*vpp)->v_type == VREG && (!UBCINFOEXISTS(*vpp))) {
        ubc_info_init(*vpp);
    }
    ubc_hold(*vpp);
#endif
    return 0;
}



/*
 * Perform an open operation on the vnode identified by a `nnpfs_fhandle_t'
 * (see nnpfs_fhlookup) with flags `user_flags'.  Returns 0 or
 * error.  If succsesful, the file descriptor is returned in `retval'.
 */

extern struct fileops vnops;	/* sometimes declared in <file.h> */

int
nnpfs_fhopen (d_thread_t *proc,
	    struct nnpfs_fhandle_t *fhp,
	    int user_flags,
	    register_t *retval)
{
    int error;
    struct vnode *vp;
#ifdef HAVE_FREEBSD_THREAD
    struct ucred *cred = proc->td_proc->p_ucred;
#else
    struct ucred *cred = proc->p_ucred;
#endif
    int flags = FFLAGS(user_flags);
    int index;
    struct file *fp;
    int mode;
    struct nnpfs_fhandle_t fh;

    NNPFSDEB(XDEBVFOPS, ("nnpfs_fhopen: flags = %d\n", user_flags));

    error = copyin (fhp, &fh, sizeof(fh));
    if (error)
	return error;

    error = nnpfs_fhlookup (proc, &fh, &vp);
    NNPFSDEB(XDEBVFOPS, ("nnpfs_fhlookup returned %d\n", error));
    if (error)
	return error;

    switch (vp->v_type) {
    case VDIR :
    case VREG :
	break;
    case VLNK :
	error = EMLINK;
	goto out;
    default :
	error = EOPNOTSUPP;
	goto out;
    }

    mode = 0;
    if (flags & FWRITE) {
	switch (vp->v_type) {
	case VREG :
	    break;
	case VDIR :
	    error = EISDIR;
	    goto out;
	default :
	    error = EOPNOTSUPP;
	    goto out;
	}

	error = vn_writechk (vp);
	if (error)
	    goto out;

	mode |= VWRITE;
    }
    if (flags & FREAD)
	mode |= VREAD;

    if (mode) {
	error = VOP_ACCESS(vp, mode, cred, proc);
	if (error)
	    goto out;
    }

    error = VOP_OPEN(vp, flags, cred, proc);
    if (error)
	goto out;

    error = falloc(proc, &fp, &index);
    if (error)
	goto out;

    if (flags & FWRITE)
        vp->v_writecount++;

#if defined(__FreeBSD_version) && __FreeBSD_version >= 300000
    if (vp->v_type == VREG) {
#ifdef HAVE_FREEBSD_THREAD
	error = nnpfs_vfs_object_create(vp, proc, proc->td_proc->p_ucred);
#else
	error = nnpfs_vfs_object_create(vp, proc, proc->p_ucred);
#endif
	if (error)
	    goto out;
    }
#endif

    fp->f_flag = flags & FMASK;
    fp->f_type = DTYPE_VNODE;
    fp->f_ops  = &vnops;
    fp->f_data = (caddr_t)vp;
    nnpfs_vfs_unlock(vp, proc);
    *retval = index;
#ifdef FILE_UNUSE
    FILE_UNUSE(fp, proc);
#endif
#ifdef __APPLE__
    *fdflags(proc, index) &= ~UF_RESERVED;
#endif
    return 0;
out:
    NNPFSDEB(XDEBVFOPS, ("nnpfs_fhopen: error = %d\n", error));
    vput(vp);
    return error;
}
