/*	$OpenBSD: xfs_vfsops.c,v 1.1 1998/08/30 16:47:22 art Exp $	*/
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
#include <sys/mount.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/proc.h>

#include <xfs/xfs_common.h>

RCSID("$KTH: xfs_vfsops.c,v 1.22 1998/08/13 01:38:49 art Exp $");

/*
 * XFS vfs operations.
 */

#include <xfs/xfs_common.h>
#include <sys/xfs_message.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_deb.h>

static struct vnode *make_dead_vnode(struct mount * mp);

struct xfs xfs[NXFS];

static int
xfs_mount(struct mount * mp,
	  const char *user_path,
	  caddr_t user_data,
	  struct nameidata * ndp,
	  struct proc * p)
{
    struct vnode *devvp;
    dev_t dev;
    int error;
    struct vattr vat;
    char path[MAXPATHLEN];
    char data[MAXPATHLEN];
    size_t len;

    error = copyinstr(user_path, path, MAXPATHLEN, &len);
    if (error)
	return error;

    error = copyinstr(user_data, data, MAXPATHLEN, &len);
    if (error)
	return error;

    XFSDEB(XDEBVFOPS, ("xfs_mount: "
		       "struct mount mp = %p path = %s data = '%s'\n",
		       mp, path, data));

    NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF,
	   UIO_SYSSPACE, data, p);
    error = namei(ndp);
    if (error)
	return error;

    devvp = ndp->ni_vp;

    if (devvp->v_type != VCHR) {
	vput(devvp);
	return ENXIO;
    }
    error = VOP_GETATTR(devvp, &vat, p->p_ucred, p);
    if (error) {
	vput(devvp);
	return error;
    }
    dev = vat.va_rdev;
    vput(devvp);

    /* Check that this device really is an xfs_dev */
    if (major(dev) < 0 || nchrdev < major(dev))
	return ENXIO;
    if (minor(dev) < 0 || NXFS < minor(dev))
	return ENXIO;
#if defined(__NetBSD__) || defined(__OpenBSD__)
    if (cdevsw[major(dev)].d_open != xfs_devopen)
	return ENXIO;
#elif defined(__FreeBSD__)
    if (cdevsw[major(dev)] == NULL
	|| cdevsw[major(dev)]->d_open != xfs_devopen)
	return ENXIO;
#endif

    if (xfs[minor(dev)].status & XFS_MOUNTED)
	return EBUSY;

    xfs[minor(dev)].status = XFS_MOUNTED;
    xfs[minor(dev)].mp = mp;
    xfs[minor(dev)].root = 0;
    xfs[minor(dev)].nnodes = 0;
    xfs[minor(dev)].fd = minor(dev);

    VFS_TO_XFS(mp) = &xfs[minor(dev)];
    vfs_getnewfsid(mp);

    mp->mnt_stat.f_bsize = DEV_BSIZE;
    mp->mnt_stat.f_iosize = DEV_BSIZE;
    mp->mnt_stat.f_blocks = 4711 * 4711;
    mp->mnt_stat.f_bfree = 4711 * 4711;
    mp->mnt_stat.f_bavail = 4711 * 4711;
    mp->mnt_stat.f_files = 4711;
    mp->mnt_stat.f_ffree = 4711;
    mp->mnt_stat.f_owner = 0;
    mp->mnt_stat.f_flags = mp->mnt_flag;

    strncpy(mp->mnt_stat.f_mntonname,
	    path,
	    sizeof(mp->mnt_stat.f_mntonname));

    strncpy(mp->mnt_stat.f_mntfromname,
	    "arla",
	    sizeof(mp->mnt_stat.f_mntfromname));

    strncpy(mp->mnt_stat.f_fstypename,
	    "xfs",
	    sizeof(mp->mnt_stat.f_fstypename));

    return 0;
}

static int
xfs_start(struct mount * mp, int flags, struct proc * p)
{
    XFSDEB(XDEBVFOPS, ("xfs_start mp = 0x%x\n", (u_int) mp));
    return 0;
}

static int
xfs_unmount(struct mount * mp, int mntflags, struct proc * p)
{
    struct xfs *xfsp = VFS_TO_XFS(mp);
    extern int doforce;
    int flags = 0;
    int error;

    XFSDEB(XDEBVFOPS, ("xfs_unmount mp = 0x%x\n", (u_int) mp));

    if (mntflags & MNT_FORCE) {
	if (!doforce)
	    return EINVAL;
	flags |= FORCECLOSE;
    }

    error = free_all_xfs_nodes(xfsp, flags);
    if (error)
	return error;

    xfsp->status = 0;

    return 0;
}

static int
xfs_root(struct mount * mp, struct vnode ** vpp)
{
    struct xfs *xfsp = VFS_TO_XFS(mp);
    struct xfs_message_getroot msg;
    int error;

    XFSDEB(XDEBVFOPS, ("xfs_root mp = 0x%x\n", (u_int) mp));

    do {
	if (xfsp->root != NULL) {
	    *vpp = XNODE_TO_VNODE(xfsp->root);
	    VREF(*vpp);
	    return 0;
	}
	msg.header.opcode = XFS_MSG_GETROOT;
	msg.cred.uid = curproc->p_ucred->cr_uid;
	msg.cred.pag = 0;	       /* XXX */
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) & msg)->error;
    } while (error == 0);
    /*
     * Failed to get message through, need to pretend that all went well
     * and return a fake dead vnode to be able to unmount.
     */
    *vpp = make_dead_vnode(mp);
    (*vpp)->v_flag |= VROOT;
    return 0;
}

static int
xfs_quotactl(struct mount * mp,
	     int cmd,
	     uid_t uid,
	     caddr_t arg,
	     struct proc * p)
{
    XFSDEB(XDEBVFOPS, ("xfs_quotactl\n"));
    return (EOPNOTSUPP);
}

static int
xfs_statfs(struct mount * mp,
	   struct statfs * sbp,
	   struct proc * p)
{
    XFSDEB(XDEBVFOPS, ("xfs_statfs\n"));

    bcopy(&mp->mnt_stat, sbp, sizeof(*sbp));
    return 0;
}

static int
xfs_sync(struct mount * mp,
	 int waitfor,
	 struct ucred * cred,
	 struct proc * p)
{
    XFSDEB(XDEBVFOPS, ("xfs_sync\n"));
    return 0;
}

static int
xfs_vget(struct mount * mp,
	 ino_t ino,
	 struct vnode ** vpp)
{
    XFSDEB(XDEBVFOPS, ("xfs_vget\n"));
    return EOPNOTSUPP;
}

static int
xfs_fhtovp(struct mount * mp,
	   struct fid * fhp,
	   struct mbuf * nam,
	   struct vnode ** vpp,
	   int *exflagsp,
	   struct ucred ** credanonp)
{
    XFSDEB(XDEBVFOPS, ("xfs_fhtovp\n"));
    return EOPNOTSUPP;
}

static int
xfs_vptofh(struct vnode * vp,
	   struct fid * fhp)
{
    XFSDEB(XDEBVFOPS, ("xfs_vptofh\n"));
    return EOPNOTSUPP;
}


/* sysctl()able variables :-) */
static int
xfs_sysctl(int *name, u_int namelen, void *oldp, size_t * oldlenp,
	   void *newp, size_t newlen, struct proc * p)
{
    /* Supposed to be terminal... */
    if (namelen != 1)
	return (ENOTDIR);

    return (EOPNOTSUPP);
}

static int
xfs_init(struct vfsconf * vfsp)
{
    XFSDEB(XDEBVFOPS, ("xfs_init\n"));

    return (0);
}


struct vfsops xfs_vfsops = {
    xfs_mount,
    xfs_start,
    xfs_unmount,
    xfs_root,
    xfs_quotactl,
    xfs_statfs,
    xfs_sync,
    xfs_vget,
    xfs_fhtovp,
    xfs_vptofh,
    xfs_init,
    xfs_sysctl
};

/*
 *
 */
static int
xfs_uprintf_filsys(void)
{
    return 0;
}

/*
 * Install and uninstall filesystem.
 */

extern struct vnodeopv_desc xfs_vnodeop_opv_desc;

int
xfs_install_filesys(void)
{

    struct vfsconf *vfsp;
    struct vfsconf **vfspp;


    /* Check if filesystem already known */
    for (vfspp = &vfsconf, vfsp = vfsconf;
	 vfsp;
	 vfspp = &vfsp->vfc_next, vfsp = vfsp->vfc_next)
	if (strncmp(vfsp->vfc_name,
		    "xfs", MFSNAMELEN) == 0)
	    return (EEXIST);

    /* Allocate and initialize */
    MALLOC(vfsp, struct vfsconf *, sizeof(struct vfsconf),
	   M_VFS, M_WAITOK);

    vfsp->vfc_vfsops = &xfs_vfsops;
    strncpy(vfsp->vfc_name, "xfs", MFSNAMELEN);
    vfsp->vfc_typenum = 0;
    vfsp->vfc_refcount = 0;
    vfsp->vfc_flags = 0;
    vfsp->vfc_mountroot = 0;
    vfsp->vfc_next = NULL;

    maxvfsconf++;

    /* Add to the end of the list */
    *vfspp = vfsp;

    /* Call vfs_init() */
    printf("Calling vfs_init()\n");
    (*(vfsp->vfc_vfsops->vfs_init)) (vfsp);

    /* done! */

    return 0;
}

int
xfs_uninstall_filesys(void)
{

    struct vfsconf *vfsp;
    struct vfsconf **vfspp;


    /* Find our vfsconf struct */
    for (vfspp = &vfsconf, vfsp = vfsconf;
	 vfsp;
	 vfspp = &vfsp->vfc_next, vfsp = vfsp->vfc_next)
	if (strncmp(vfsp->vfc_name,
		    "xfs",
		    MFSNAMELEN) == 0)
	    break;

    if (!vfsp)			       /* Not found */
	return (EEXIST);

    if (vfsp->vfc_refcount)	       /* In use */
	return (EBUSY);

    /* Remove from list and free  */
    *vfspp = vfsp->vfc_next;
    FREE(vfsp, M_VFS);

    maxvfsconf--;

    return 0;
}



int
xfs_stat_filesys(void)
{
    return xfs_uprintf_filsys();
}

/*
 * To be able to unmount when the XFS daemon is not
 * responding we need a root vnode, use a dead vnode!
 */
extern int (**dead_vnodeop_p) (void *);

static struct vnode *
make_dead_vnode(struct mount * mp)
{
    struct vnode *dead;
    int error;

    XFSDEB(XDEBNODE, ("make_dead_vnode mp = 0x%x\n", (u_int) mp));

    if ((error = getnewvnode(VT_NON, mp, dead_vnodeop_p, &dead)))
	panic("make_dead_vnode: getnewvnode failed: error = %d\n", error);

    return dead;
}
