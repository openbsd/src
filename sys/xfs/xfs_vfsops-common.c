/*	$OpenBSD: xfs_vfsops-common.c,v 1.2 2000/03/03 00:54:59 todd Exp $	*/

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

RCSID("$OpenBSD: xfs_vfsops-common.c,v 1.2 2000/03/03 00:54:59 todd Exp $");

/*
 * XFS vfs operations.
 */

#include <xfs/xfs_common.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_deb.h>
#include <xfs/xfs_syscalls.h>
#include <xfs/xfs_vfsops.h>

struct xfs xfs[NXFS];

int
xfs_mount_common(struct mount *mp,
		 const char *user_path,
		 caddr_t user_data,
		 struct nameidata *ndp,
		 struct proc *p)
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
		       "struct mount mp = %p path = '%s' data = '%s'\n",
		       mp, path, data));

#ifdef ARLA_KNFS
    XFSDEB(XDEBVFOPS, ("xfs_mount: mount flags = %x\n", mp->mnt_flag));

    /*
     * mountd(8) flushes all export entries when it starts
     * right now we ignore it (but should not)
     */

    if (mp->mnt_flag & MNT_UPDATE ||
	mp->mnt_flag & MNT_DELEXPORT) {

	XFSDEB(XDEBVFOPS, 
	       ("xfs_mount: ignoreing MNT_UPDATE or MNT_DELEXPORT\n"));
	return 0;
    }
#endif

    NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, data, p);
    error = namei(ndp);
    if (error) {
	XFSDEB(XDEBVFOPS, ("namei failed, errno = %d\n", error));
	return error;
    }

    devvp = ndp->ni_vp;

    if (devvp->v_type != VCHR) {
	vput(devvp);
	XFSDEB(XDEBVFOPS, ("not VCHR (%d)\n", devvp->v_type));
	return ENXIO;
    }
#ifdef __osf__
    VOP_GETATTR(devvp, &vat, ndp->ni_cred, error);
#else
    error = VOP_GETATTR(devvp, &vat, p->p_ucred, p);
#endif
    vput(devvp);
    if (error) {
	XFSDEB(XDEBVFOPS, ("VOP_GETATTR failed, error = %d\n", error));
	return error;
    }
    dev = vat.va_rdev;
    XFSDEB(XDEBVFOPS, ("dev = %d.%d\n", major(dev), minor(dev)));

    /* Check that this device really is an xfs_dev */
    if (major(dev) < 0 || major(dev) > nchrdev) {
	XFSDEB(XDEBVFOPS, ("major out of range (0 < %d < %d)\n", 
			   major(dev), nchrdev));
	return ENXIO;
    }
    if (minor(dev) < 0 || NXFS < minor(dev)) {
	XFSDEB(XDEBVFOPS, ("minor out of range (0 < %d < %d)\n", 
			   minor(dev), NXFS));
	return ENXIO;
    }
#if defined(__NetBSD__) || defined(__OpenBSD__)
    if(!xfs_func_is_devopen(cdevsw[major(dev)].d_open))
	return ENXIO;
#elif defined(__FreeBSD__)
    if (cdevsw[major(dev)] == NULL
	|| !xfs_func_is_devopen(cdevsw[major(dev)]->d_open))
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
#if defined(HAVE_KERNEL_VFS_GETNEWFSID)
#if defined(HAVE_TWO_ARGUMENT_VFS_GETNEWFSID)
    vfs_getnewfsid(mp, MOUNT_AFS);
#else
    vfs_getnewfsid(mp);
#endif
#elif defined(__NetBSD__) || defined(__OpenBSD__)
    getnewfsid(mp, makefstype(MOUNT_AFS));
#elif defined(__FreeBSD__)
    getnewfsid(mp, MOUNT_AFS);
    mp->mnt_stat.f_type = MOUNT_AFS;
#endif

    mp->mnt_stat.f_bsize = DEV_BSIZE;
#ifndef __osf__
    mp->mnt_stat.f_iosize = DEV_BSIZE;
    mp->mnt_stat.f_owner = 0;
#endif
    mp->mnt_stat.f_blocks = 4711 * 4711;
    mp->mnt_stat.f_bfree = 4711 * 4711;
    mp->mnt_stat.f_bavail = 4711 * 4711;
    mp->mnt_stat.f_files = 4711;
    mp->mnt_stat.f_ffree = 4711;
    mp->mnt_stat.f_flags = mp->mnt_flag;

#ifdef __osf__
    mp->mnt_stat.f_fsid.val[0] = dev;
    mp->mnt_stat.f_fsid.val[1] = MOUNT_XFS;
	
    MALLOC(mp->m_stat.f_mntonname, char *, strlen(path) + 1, 
	   M_PATHNAME, M_WAITOK);
    strcpy(mp->m_stat.f_mntonname, path);

    MALLOC(mp->m_stat.f_mntfromname, char *, sizeof("arla"),
	   M_PATHNAME, M_WAITOK);
    strcpy(mp->m_stat.f_mntfromname, "arla");
#else /* __osf__ */
    strncpy(mp->mnt_stat.f_mntonname,
	    path,
	    sizeof(mp->mnt_stat.f_mntonname));

    strncpy(mp->mnt_stat.f_mntfromname,
	    "arla",
	    sizeof(mp->mnt_stat.f_mntfromname));

#if defined(__NetBSD__) || defined(__OpenBSD__)
    strncpy(mp->mnt_stat.f_fstypename,
	    "xfs",
	    sizeof(mp->mnt_stat.f_fstypename));
#endif
#endif /* __osf__ */

    return 0;
}

int
xfs_unmount_common(struct mount *mp, int mntflags)
{
    struct xfs *xfsp = VFS_TO_XFS(mp);
    int flags = 0;
    int error;

    if (mntflags & MNT_FORCE) {
#ifdef HAVE_KERNEL_DOFORCE
	extern int doforce;
	if (!doforce)
	    return EINVAL;
#endif
	flags |= FORCECLOSE;
    }

    error = free_all_xfs_nodes(xfsp, flags, 1);
    if (error)
	return error;

    xfsp->status = 0;
    XFS_TO_VFS(xfsp) = NULL;
    return 0;
}

int
xfs_root_common(struct mount *mp, struct vnode **vpp,
		struct proc *proc, struct ucred *cred)
{
    struct xfs *xfsp = VFS_TO_XFS(mp);
    struct xfs_message_getroot msg;
    int error;

    do {
	if (xfsp->root != NULL) {
	    *vpp = XNODE_TO_VNODE(xfsp->root);
	    xfs_do_vget(*vpp, LK_INTERLOCK|LK_EXCLUSIVE, proc);
	    return 0;
	}
	msg.header.opcode = XFS_MSG_GETROOT;
	msg.cred.uid = cred->cr_uid;
	msg.cred.pag = xfs_get_pag(cred);
	error = xfs_message_rpc(xfsp->fd, &msg.header, sizeof(msg));
	if (error == 0)
	    error = ((struct xfs_message_wakeup *) & msg)->error;
    } while (error == 0);
    /*
     * Failed to get message through, need to pretend that all went well
     * and return a fake dead vnode to be able to unmount.
     */

    if ((error = make_dead_vnode(mp, vpp)))
	return error;

    (*vpp)->v_flag |= VROOT;
    return 0;
}
