/*	$OpenBSD: vfs_syscalls_43.c,v 1.21 2002/10/03 00:07:20 nordin Exp $	*/
/*	$NetBSD: vfs_syscalls_43.c,v 1.4 1996/03/14 19:31:52 christos Exp $	*/

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
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include <sys/pipe.h>

static void cvtstat(struct stat *, struct ostat *);

/*
 * Redirection info so we don't have to include the union fs routines in 
 * the kernel directly.  This way, we can build unionfs as an LKM.  The
 * pointer gets replaced later, when we modload the LKM, or when the
 * compiled-in unionfs code gets initialized.  Initial, stub routine
 * value is compiled in from kern/vfs_syscalls.c
 */

extern int (*union_check_p)(struct proc *, struct vnode **, 
				   struct file *, struct uio, int *);

/*
 * Convert from an old to a new stat structure.
 */
static void
cvtstat(st, ost)
	struct stat *st;
	struct ostat *ost;
{

	ost->st_dev = st->st_dev;
	ost->st_ino = st->st_ino;
	ost->st_mode = st->st_mode;
	ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid;
	ost->st_gid = st->st_gid;
	ost->st_rdev = st->st_rdev;
	if (st->st_size < (quad_t)1 << 32)
		ost->st_size = st->st_size;
	else
		ost->st_size = -2;
	ost->st_atime = st->st_atime;
	ost->st_mtime = st->st_mtime;
	ost->st_ctime = st->st_ctime;
	ost->st_blksize = st->st_blksize;
	ost->st_blocks = st->st_blocks;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}

/*
 * Get file status; this version follows links.
 */
/* ARGSUSED */
int
compat_43_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ostat *) ub;
	} */ *uap = v;
	struct stat sb;
	struct ostat osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)SCARG(uap, ub), sizeof (osb));
	return (error);
}


/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
int
compat_43_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ostat *) ub;
	} */ *uap = v;
	struct stat sb;
	struct ostat osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout(&osb, SCARG(uap, ub), sizeof (osb));
	return (error);
}


/*
 * Return status information about a file descriptor.
 */
/* ARGSUSED */
int
compat_43_sys_fstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_sys_fstat_args /* {
		syscallarg(int) fd;
		syscallarg(struct ostat *) sb;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct stat ub;
	struct ostat oub;
	int error;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	FREF(fp);
	error = (*fp->f_ops->fo_stat)(fp, &ub, p);
	FRELE(fp);
	cvtstat(&ub, &oub);
	if (error == 0)
		error = copyout((caddr_t)&oub, (caddr_t)SCARG(uap, sb),
		    sizeof (oub));
	return (error);
}


/*
 * Truncate a file given a file descriptor.
 */
/* ARGSUSED */
int
compat_43_sys_ftruncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_ftruncate_args /* {
		syscallarg(int) fd;
		syscallarg(long) length;
	} */ *uap = v;
	struct sys_ftruncate_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ nuap;

	SCARG(&nuap, fd) = SCARG(uap, fd);
	SCARG(&nuap, length) = SCARG(uap, length);
	return (sys_ftruncate(p, &nuap, retval));
}

/*
 * Truncate a file given its path name.
 */
/* ARGSUSED */
int
compat_43_sys_truncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(long) length;
	} */ *uap = v;
	struct sys_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ nuap;

	SCARG(&nuap, path) = SCARG(uap, path);
	SCARG(&nuap, length) = SCARG(uap, length);
	return (sys_truncate(p, &nuap, retval));
}


/*
 * Reposition read/write file offset.
 */
int
compat_43_sys_lseek(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_lseek_args /* {
		syscallarg(int) fd;
		syscallarg(long) offset;
		syscallarg(int) whence;
	} */ *uap = v;
	struct sys_lseek_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
		syscallarg(int) whence;
	} */ nuap;
	off_t qret;
	int error;

	SCARG(&nuap, fd) = SCARG(uap, fd);
	SCARG(&nuap, offset) = SCARG(uap, offset);
	SCARG(&nuap, whence) = SCARG(uap, whence);
	error = sys_lseek(p, &nuap, (register_t *)&qret);
	*(long *)retval = qret;
	return (error);
}


/*
 * Create a file.
 */
int
compat_43_sys_creat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_creat_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	struct sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ nuap;

	SCARG(&nuap, path) = SCARG(uap, path);
	SCARG(&nuap, mode) = SCARG(uap, mode);
	SCARG(&nuap, flags) = O_WRONLY | O_CREAT | O_TRUNC;
	return (sys_open(p, &nuap, retval));
}

/*ARGSUSED*/
int
compat_43_sys_quota(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return (ENOSYS);
}


/*
 * Read a block of directory entries in a file system independent format.
 */
int
compat_43_sys_getdirentries(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_getdirentries_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(int) count;
		syscallarg(long *) basep;
	} */ *uap = v;
	struct vnode *vp;
	struct file *fp;
	struct uio auio, kuio;
	struct iovec aiov, kiov;
	struct dirent *dp, *edp;
	caddr_t dirbuf;
	int error, eofflag, readcnt;
	long loff;

	if (SCARG(uap, count) < 0)
		return EINVAL;
	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto bad;
	}
	vp = (struct vnode *)fp->f_data;
unionread:
	if (vp->v_type != VDIR) {
		error = EINVAL;
		goto bad;
	}
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, count);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	auio.uio_resid = SCARG(uap, count);
       
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	loff = auio.uio_offset = fp->f_offset;
#	if (BYTE_ORDER != LITTLE_ENDIAN)
		if (vp->v_mount->mnt_maxsymlinklen <= 0) {
			error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag,
			    (int *)0, (u_long **)0);
			fp->f_offset = auio.uio_offset;
		} else
#	endif
	{
		u_int  nbytes = SCARG(uap, count);

		nbytes = min(nbytes, MAXBSIZE);

		kuio = auio;
		kuio.uio_iov = &kiov;
		kuio.uio_segflg = UIO_SYSSPACE;
		kiov.iov_len = nbytes;
		dirbuf = (caddr_t)malloc(nbytes, M_TEMP, M_WAITOK);
		kiov.iov_base = dirbuf;

		error = VOP_READDIR(vp, &kuio, fp->f_cred, &eofflag,
				    0, 0);
		fp->f_offset = kuio.uio_offset;
		if (error == 0) {
			readcnt = nbytes - kuio.uio_resid;
			edp = (struct dirent *)&dirbuf[readcnt];
			for (dp = (struct dirent *)dirbuf; dp < edp; ) {
#				if (BYTE_ORDER == LITTLE_ENDIAN)
					/*
					 * The expected low byte of
					 * dp->d_namlen is our dp->d_type.
					 * The high MBZ byte of dp->d_namlen
					 * is our dp->d_namlen.
					 */
					dp->d_type = dp->d_namlen;
					dp->d_namlen = 0;
#				else
					/*
					 * The dp->d_type is the high byte
					 * of the expected dp->d_namlen,
					 * so must be zero'ed.
					 */
					dp->d_type = 0;
#				endif
				if (dp->d_reclen > 0) {
					dp = (struct dirent *)
					    ((char *)dp + dp->d_reclen);
				} else {
					error = EIO;
					break;
				}
			}
			if (dp >= edp)
				error = uiomove(dirbuf, readcnt, &auio);
		}
		FREE(dirbuf, M_TEMP);
	}
	VOP_UNLOCK(vp, 0, p);
	if (error)
		goto bad;
	if ((SCARG(uap, count) == auio.uio_resid) &&
	    union_check_p &&
	    (union_check_p(p, &vp, fp, auio, &error) != 0))
		goto unionread;
	if (error)
		goto bad;

	if ((SCARG(uap, count) == auio.uio_resid) &&
	    (vp->v_flag & VROOT) &&
	    (vp->v_mount->mnt_flag & MNT_UNION)) {
		struct vnode *tvp = vp;
		vp = vp->v_mount->mnt_vnodecovered;
		VREF(vp);
		fp->f_data = (caddr_t) vp;
		fp->f_offset = 0;
		vrele(tvp);
		goto unionread;
	}
	error = copyout((caddr_t)&loff, (caddr_t)SCARG(uap, basep),
	    sizeof(long));
	*retval = SCARG(uap, count) - auio.uio_resid;
bad:
	FRELE(fp);
	return (error);
}
