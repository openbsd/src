/*	$OpenBSD: netbsd_getdents.c,v 1.7 2002/08/23 15:39:31 art Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)vfs_vnops.c	8.14 (Berkeley) 6/15/95
 *	@(#)vfs_syscalls.c	8.42 (Berkeley) 7/31/95
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/stat.h>

#include <compat/netbsd/netbsd_types.h>
#include <compat/netbsd/netbsd_signal.h>
#include <compat/netbsd/netbsd_syscallargs.h>

static int netbsd_vn_readdir(struct file *, char *, int, u_int, int *,
	struct proc *, off_t **, int *);

static int
netbsd_vn_readdir(fp, buf, segflg, count, done, p, cookies, ncookies)
	struct file *fp;
	char *buf;
	int segflg, *done, *ncookies;
	u_int count;
	struct proc *p;
	off_t **cookies;
{
	struct vnode *vp = (struct vnode *)fp->f_data;
	struct iovec aiov;
	struct uio auio;
	int error, eofflag;

unionread:
	if (vp->v_type != VDIR)
		return (EINVAL);
	aiov.iov_base = buf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = segflg;
	auio.uio_procp = p;
	auio.uio_resid = count;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	auio.uio_offset = fp->f_offset;
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, ncookies,
		    (u_long **)cookies); /* XXX 32-bit? */
	fp->f_offset = auio.uio_offset;
	VOP_UNLOCK(vp, 0, p);
	if (error)
		return (error);

#ifdef UNION
{
	extern int (**union_vnodeop_p)(void *);
	extern struct vnode *union_dircache(struct vnode *);

	if (count == auio.uio_resid && (vp->v_op == union_vnodeop_p)) {
		struct vnode *lvp;

		lvp = union_dircache(vp);
		if (lvp != NULLVP) {
			struct vattr va;

			/*
			 * If the directory is opaque,
			 * then don't show lower entries
			 */
			error = VOP_GETATTR(vp, &va, fp->f_cred, p);
			if (va.va_flags & OPAQUE) {
				vput(lvp);
				lvp = NULL;
			}
		}
		
		if (lvp != NULLVP) {
			error = VOP_OPEN(lvp, FREAD, fp->f_cred, p);
			if (error) {
				vput(lvp);
				return (error);
			}
			VOP_UNLOCK(lvp, 0, p);
			fp->f_data = (caddr_t) lvp;
			fp->f_offset = 0;
			error = vn_close(vp, FREAD, fp->f_cred, p);
			if (error)
				return (error);
			vp = lvp;
			goto unionread;
		}
	}
}
#endif /* UNION */

	if (count == auio.uio_resid && (vp->v_flag & VROOT) &&
	    (vp->v_mount->mnt_flag & MNT_UNION)) {
		struct vnode *tvp = vp;
		vp = vp->v_mount->mnt_vnodecovered;
		VREF(vp);
		fp->f_data = (caddr_t) vp;
		fp->f_offset = 0;
		vrele(tvp);
		goto unionread;
	}
	*done = count - auio.uio_resid;
	return error;
}

/*
 * Read a block of directory entries in a file system independent format.
 */
int
netbsd_sys_getdents(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct netbsd_sys_getdents_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(size_t) count;
	} */ *uap = v;
	struct file *fp;
	int error, done;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto bad;
	}
	error = netbsd_vn_readdir(fp, SCARG(uap, buf), UIO_USERSPACE,
			SCARG(uap, count), &done, p, 0, 0);
	*retval = done;
bad:
	FRELE(fp);
	return (error);
}

