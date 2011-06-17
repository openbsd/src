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

RCSID("$arla: nnpfs_vfsops-common.c,v 1.40 2003/06/02 18:26:40 lha Exp $");

/*
 * NNPFS vfs operations.
 */

#include <nnpfs/nnpfs_common.h>
#include <nnpfs/nnpfs_message.h>
#include <nnpfs/nnpfs_fs.h>
#include <nnpfs/nnpfs_dev.h>
#include <nnpfs/nnpfs_deb.h>
#include <nnpfs/nnpfs_syscalls.h>
#include <nnpfs/nnpfs_vfsops.h>

#ifdef HAVE_KERNEL_UDEV2DEV
#define VA_RDEV_TO_DEV(x) udev2dev(x, 0) /* XXX what is the 0 */
#else
#define VA_RDEV_TO_DEV(x) x
#endif


struct nnpfs nnpfs[NNNPFS];

/*
 * path and data is in system memory
 */

int
nnpfs_mount_common_sys(struct mount *mp,
		     const char *path,
		     void *data,
		     struct nameidata *ndp,
		     d_thread_t *p)
{
    struct vnode *devvp;
    dev_t dev;
    int error;
    struct vattr vat;

    NNPFSDEB(XDEBVFOPS, ("nnpfs_mount: "
		       "struct mount mp = %lx path = '%s' data = '%s'\n",
		       (unsigned long)mp, path, (char *)data));

#ifdef ARLA_KNFS
    NNPFSDEB(XDEBVFOPS, ("nnpfs_mount: mount flags = %x\n", mp->mnt_flag));

    /*
     * mountd(8) flushes all export entries when it starts
     * right now we ignore it (but should not)
     */

    if (mp->mnt_flag & MNT_UPDATE ||
	mp->mnt_flag & MNT_DELEXPORT) {

	NNPFSDEB(XDEBVFOPS, 
	       ("nnpfs_mount: ignoring MNT_UPDATE or MNT_DELEXPORT\n"));
	return 0;
    }
#endif

    NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, data, p);
    error = namei(ndp);
    if (error) {
	NNPFSDEB(XDEBVFOPS, ("namei failed, errno = %d\n", error));
	return error;
    }

    devvp = ndp->ni_vp;

    if (devvp->v_type != VCHR) {
	vput(devvp);
	NNPFSDEB(XDEBVFOPS, ("not VCHR (%d)\n", devvp->v_type));
	return ENXIO;
    }
#if defined(__osf__)
    VOP_GETATTR(devvp, &vat, ndp->ni_cred, error);
#elif defined(HAVE_FREEBSD_THREAD)
    error = VOP_GETATTR(devvp, &vat, p->td_proc->p_ucred, p);
#else
    error = VOP_GETATTR(devvp, &vat, p->p_ucred, p);
#endif
    vput(devvp);
    if (error) {
	NNPFSDEB(XDEBVFOPS, ("VOP_GETATTR failed, error = %d\n", error));
	return error;
    }

    dev = VA_RDEV_TO_DEV(vat.va_rdev);

    NNPFSDEB(XDEBVFOPS, ("dev = %d.%d\n", major(dev), minor(dev)));

    if (!nnpfs_is_nnpfs_dev (dev)) {
	NNPFSDEB(XDEBVFOPS, ("%s is not a nnpfs device\n", (char *)data));
	return ENXIO;
    }

    if (nnpfs[minor(dev)].status & NNPFS_MOUNTED)
	return EBUSY;

    nnpfs[minor(dev)].status = NNPFS_MOUNTED;
    nnpfs[minor(dev)].mp = mp;
    nnpfs[minor(dev)].root = 0;
    nnpfs[minor(dev)].nnodes = 0;
    nnpfs[minor(dev)].fd = minor(dev);

    nnfs_init_head(&nnpfs[minor(dev)].nodehead);

    VFS_ASSIGN(mp, &nnpfs[minor(dev)]);
#if defined(HAVE_KERNEL_VFS_GETNEWFSID)
#if defined(HAVE_TWO_ARGUMENT_VFS_GETNEWFSID)
    vfs_getnewfsid(mp, MOUNT_AFS);
#else
    vfs_getnewfsid(mp);
#endif /* HAVE_TWO_ARGUMENT_VFS_GETNEWFSID */
#endif /* HAVE_KERNEL_VFS_GETNEWFSID */

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
    mp->mnt_stat.f_fsid.val[1] = MOUNT_NNPFS;
	
    mp->m_stat.f_mntonname = malloc(strlen(path) + 1, M_PATHNAME, M_WAITOK);
    strcpy(mp->m_stat.f_mntonname, path);

    mp->m_stat.f_mntfromname = malloc(sizeof("arla"), M_PATHNAME, M_WAITOK);
    strcpy(mp->m_stat.f_mntfromname, "arla");
#else /* __osf__ */
    strncpy(mp->mnt_stat.f_mntonname,
	    path,
	    sizeof(mp->mnt_stat.f_mntonname));

    strncpy(mp->mnt_stat.f_mntfromname,
	    data,
	    sizeof(mp->mnt_stat.f_mntfromname));

    strncpy(mp->mnt_stat.f_fstypename,
	    "nnpfs",
	    sizeof(mp->mnt_stat.f_fstypename));
#endif /* __osf__ */

    return 0;
}

int
nnpfs_mount_common(struct mount *mp,
		 const char *user_path,
		 void *user_data,
		 struct nameidata *ndp,
		 d_thread_t *p)
{
    char *path = NULL;
    char *data = NULL;
    size_t count;
    int error = 0;

    data = malloc(MAXPATHLEN, M_TEMP, M_WAITOK | M_CANFAIL);
    if (data == NULL) {
        error = ENOMEM;
	goto done;
    }
    path = malloc(MAXPATHLEN, M_TEMP, M_WAITOK | M_CANFAIL);
    if (path == NULL) {
        error = ENOMEM;
	goto done;
    }

    error = copyinstr(user_path, path, MAXPATHLEN, &count);
    if (error)
        goto done;      

    error = copyinstr(user_data, data, MAXPATHLEN, &count);
    if (error)
	goto done;
    error = nnpfs_mount_common_sys (mp, path, data, ndp, p);
done:
    free(data, M_TEMP);
    free(path, M_TEMP);		   	
    return(error);	
}

#ifdef HAVE_KERNEL_DOFORCE
extern int doforce;
#endif

int
nnpfs_unmount_common(struct mount *mp, int mntflags)
{
    struct nnpfs *nnpfsp = VFS_TO_NNPFS(mp);
    int flags = 0;
    int error;

    if (mntflags & MNT_FORCE) {
#ifdef HAVE_KERNEL_DOFORCE
	if (!doforce)
	    return EINVAL;
#endif
	flags |= FORCECLOSE;
    }

    error = free_all_nnpfs_nodes(nnpfsp, flags, 1);
    if (error)
	return error;

    nnpfsp->status = 0;
    NNPFS_TO_VFS(nnpfsp) = NULL;
    return 0;
}

int
nnpfs_root_common(struct mount *mp, struct vnode **vpp,
		d_thread_t *proc, struct ucred *cred)
{
    struct nnpfs *nnpfsp = VFS_TO_NNPFS(mp);
    struct nnpfs_message_getroot msg;
    int error;

    do {
	if (nnpfsp->root != NULL) {
	    *vpp = XNODE_TO_VNODE(nnpfsp->root);
	    nnpfs_do_vget(*vpp, LK_EXCLUSIVE, proc);
	    return 0;
	}
	msg.header.opcode = NNPFS_MSG_GETROOT;
	msg.cred.uid = cred->cr_uid;
	msg.cred.pag = nnpfs_get_pag(cred);
	error = nnpfs_message_rpc(nnpfsp->fd, &msg.header, sizeof(msg), proc);
	if (error == 0)
	    error = ((struct nnpfs_message_wakeup *) & msg)->error;
    } while (error == 0);
    /*
     * Failed to get message through, need to pretend that all went well
     * and return a fake dead vnode to be able to unmount.
     */

    if ((error = nnpfs_make_dead_vnode(mp, vpp)))
	return error;

    NNPFS_MAKE_VROOT(*vpp);
    return 0;
}
