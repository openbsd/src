/*	$OpenBSD: netbsd_misc.c,v 1.8 2000/01/31 19:57:21 deraadt Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_fork.c	8.6 (Berkeley) 4/8/94
 *	@(#)vfs_syscalls.c	8.28 (Berkeley) 12/10/94
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/vnode.h>

#include <compat/netbsd/netbsd_types.h>
#include <compat/netbsd/netbsd_signal.h>
#include <compat/netbsd/netbsd_syscallargs.h>

/* XXX doesn't do shared address space */
/*ARGSUSED*/
int
netbsd_sys___vfork14(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	/* XXX - should add FORK_SHAREVM */
	return (fork1(p, FORK_VFORK|FORK_PPWAIT, NULL, 0, retval));
}

/* XXX syncs whole file */
/*ARGSUSED*/
int
netbsd_sys_fdatasync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct netbsd_sys_fdatasync_args /* {
		syscallarg(int) fd;
	} */ *uap = v;
	register struct vnode *vp;
	struct file *fp;
	int error;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = (struct vnode *)fp->f_data;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if ((error = VOP_FSYNC(vp, fp->f_cred, MNT_WAIT, p)) == 0 &&
	    bioops.io_fsync != NULL)
		error = (*bioops.io_fsync)(vp);

	VOP_UNLOCK(vp, 0, p);
	return (error);
}

/*ARGSUSED*/
int
netbsd_sys_lchmod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct netbsd_sys_lchmod_args /* {
		syscallarg(char *) path;
		syscallarg(netbsd_mode_t) mode;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	if (SCARG(uap, mode) & ~(S_IFMT | ALLPERMS))
		return (EINVAL);

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		VATTR_NULL(&vattr);
		vattr.va_mode = SCARG(uap, mode) & ALLPERMS;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*ARGSUSED*/
int
netbsd_sys_lutimes(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
        register struct netbsd_sys_lutimes_args /* {
                syscallarg(const char *) path;
                syscallarg(const struct timeval *) tptr;
        } */ *uap = v;
        register struct vnode *vp;
        struct timeval tv[2];
        struct vattr vattr;
        int error;
        struct nameidata nd;

        VATTR_NULL(&vattr);
        if (SCARG(uap, tptr) == NULL) {
                microtime(&tv[0]);
                tv[1] = tv[0];
                vattr.va_vaflags |= VA_UTIMES_NULL;
        } else {
                error = copyin((caddr_t)SCARG(uap, tptr), (caddr_t)tv,
                               sizeof (tv));
                if (error)
                        return (error);
		/* XXX workaround timeval matching the VFS constant VNOVAL */
		if (tv[0].tv_sec == VNOVAL)
			tv[0].tv_sec = VNOVAL - 1;
		if (tv[1].tv_sec == VNOVAL)
			tv[1].tv_sec = VNOVAL - 1;
        }
        NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
        if ((error = namei(&nd)) != 0)
                return (error);
        vp = nd.ni_vp;
        VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
        if (vp->v_mount->mnt_flag & MNT_RDONLY)
                error = EROFS;
        else {
                vattr.va_atime.tv_sec = tv[0].tv_sec;
                vattr.va_atime.tv_nsec = tv[0].tv_usec * 1000;
                vattr.va_mtime.tv_sec = tv[1].tv_sec;
                vattr.va_mtime.tv_nsec = tv[1].tv_usec * 1000;
                error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
        }
	vput(vp);
        return (error);
}
