/*	$OpenBSD: netbsd_pos_io.c,v 1.2 1999/09/17 12:37:54 kstailey Exp $	*/

/*	$NetBSD: vfs_syscalls.c,v 1.71 1996/04/23 10:29:02 mycroft Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
 *	@(#)vfs_syscalls.c	8.28 (Berkeley) 12/10/94
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/uio.h>
/*#include <sys/malloc.h>*/
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <compat/netbsd/netbsd_types.h>
#include <compat/netbsd/netbsd_signal.h>
#include <compat/netbsd/netbsd_syscallargs.h>

static int netbsd_set_pos __P((struct proc *, int fd, off_t offset));
/*
 * sys_lseek trimmed down
 */
static int
netbsd_set_pos(p, fd, offset)
	struct proc *p;
	int fd;
	off_t offset;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	int special;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (ESPIPE);
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type == VFIFO)
		return (ESPIPE);
	if (vp->v_type == VCHR)
		special = 1;
	else
		special = 0;
	if (!special && offset < 0)
		return (EINVAL);
	fp->f_offset = offset;
	return (0);
}

/*
 * Positional read system call.
 */
/* ARGSUSED */
int
netbsd_sys_pread(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct netbsd_sys_pread_args /* {
		syscallarg(int) fd;
		syscallarg(void *) buf;
		syscallarg(size_t) nbyte;
		syscallarg(off_t) offset;
	} */ *uap = v;
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	struct iovec aiov;
	long cnt, error = 0;
#ifdef KTRACE
	struct iovec ktriov;
#endif

	if ((error = netbsd_set_pos(p, SCARG(uap, fd), SCARG(uap, offset))))
		return (error);
	fp = fdp->fd_ofiles[SCARG(uap, fd)];
	/* Don't allow nbyte to be larger than max return val */
	if (SCARG(uap, nbyte) > SSIZE_MAX)
		return(EINVAL);
	aiov.iov_base = (caddr_t)SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, nbyte);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = SCARG(uap, nbyte);
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))
		ktriov = aiov;
#endif
	cnt = SCARG(uap, nbyte);
	error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred);
	if (error)
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO) && error == 0)
		ktrgenio(p->p_tracep, SCARG(uap, fd), UIO_READ, &ktriov,
		    cnt, error);
#endif
	*retval = cnt;
	return (error);
}

/*
 * Positional scatter read system call.
 */
/* ARGSUSED */
int
netbsd_sys_preadv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct netbsd_sys_preadv_args /* {
		syscallarg(int) fd;
		syscallarg(const struct iovec *) iovp;
		syscallarg(int) iovcnt;
		syscallarg(off_t) offset;
	} */ *uap = v;
#else
	return (0);
#endif
}

/*
 * Positional write system call.
 */
/* ARGSUSED */
int
netbsd_sys_pwrite(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct netbsd_sys_pwrite_args /* {
		syscallarg(int) fd;
		syscallarg(const void *) buf;
		syscallarg(size_t) nbyte;
		syscallarg(off_t) offset;
	} */ *uap = v;
#else
	return (0);
#endif
}

/*
 * Positional gather write system call.
 */
/* ARGSUSED */
int
netbsd_sys_pwritev(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct netbsd_sys_pwritev_args /* {
		syscallarg(int) fd;
		syscallarg(const struct iovec *) iovp;
		syscallarg(int) iovcnt;
		syscallarg(off_t) offset;
	} */ *uap = v;
#else
	return (0);
#endif
}
