/*	$OpenBSD: netbsd_stat.c,v 1.12 2001/10/26 12:03:27 art Exp $	*/
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
 *	@(#)vfs_syscalls.c	8.42 (Berkeley) 7/31/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/pipe.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>

#include <sys/syscallargs.h>

#include <compat/netbsd/netbsd_types.h>
#include <compat/netbsd/netbsd_stat.h>
#include <compat/netbsd/netbsd_signal.h>
#include <compat/netbsd/netbsd_syscallargs.h>
#include <compat/netbsd/netbsd_util.h>

static void openbsd_to_netbsd_stat __P((struct stat *, struct netbsd_stat *));

static void
openbsd_to_netbsd_stat(obst, nbst)
	struct stat		*obst;
	struct netbsd_stat 	*nbst;
{
	bzero(nbst, sizeof(*nbst));
	nbst->st_dev = obst->st_dev;
	nbst->st_ino = obst->st_ino;
	nbst->st_mode = obst->st_mode;
	nbst->st_nlink = obst->st_nlink;
	nbst->st_uid = obst->st_uid;
	nbst->st_gid = obst->st_gid;
	nbst->st_rdev = obst->st_rdev;
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
	nbst->st_atimespec.tv_sec  = obst->st_atimespec.tv_sec;
	nbst->st_atimespec.tv_nsec = obst->st_atimespec.tv_nsec;
	nbst->st_mtimespec.tv_sec  = obst->st_mtimespec.tv_sec;
	nbst->st_mtimespec.tv_nsec = obst->st_mtimespec.tv_nsec;
	nbst->st_ctimespec.tv_sec  = obst->st_ctimespec.tv_sec;
	nbst->st_ctimespec.tv_nsec = obst->st_ctimespec.tv_nsec;
#else
	nbst->st_atime     = obst->st_atime;
	nbst->st_atimensec = obst->st_atimensec;
	nbst->st_mtime     = obst->st_mtime;
	nbst->st_mtimensec = obst->st_mtimensec;
	nbst->st_ctime     = obst->st_ctime;
	nbst->st_ctimensec = obst->st_ctimensec;
#endif
	nbst->st_size = obst->st_size;
	nbst->st_blocks = obst->st_blocks;
	nbst->st_blksize = obst->st_blksize;
	nbst->st_flags = obst->st_flags;
	nbst->st_gen = obst->st_gen;
	bcopy(obst->st_qspare, nbst->st_qspare, sizeof(obst->st_qspare));
}

/*
 * Get file status; this version follows links.
 */
/* ARGSUSED */
int
netbsd_sys___stat13(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct netbsd_sys___stat13_args /* {
		syscallarg(char *) path;
		syscallarg(struct netbsd_stat *) nsb;
	} */ *uap = v;
	struct netbsd_stat nsb;
	struct stat sb;
	int error;
	struct nameidata nd;
	caddr_t sg = stackgap_init(p->p_emul);

	NETBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	/* Don't let non-root see generation numbers (for NFS security) */
	if (suser(p->p_ucred, &p->p_acflag))
		sb.st_gen = 0;
	openbsd_to_netbsd_stat(&sb, &nsb);
	error = copyout(&nsb, SCARG(uap, ub), sizeof(nsb));
	return (error);
}

/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
int
netbsd_sys___lstat13(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct netbsd_sys___lstat13_args /* {
		syscallarg(char *) path;
		syscallarg(struct netbsd_stat *) ub;
	} */ *uap = v;
	struct netbsd_stat nsb;
	struct stat sb;
	int error;
	struct nameidata nd;
	caddr_t sg = stackgap_init(p->p_emul);

	NETBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	/* Don't let non-root see generation numbers (for NFS security) */
	if (suser(p->p_ucred, &p->p_acflag))
		sb.st_gen = 0;
	openbsd_to_netbsd_stat(&sb, &nsb);
	error = copyout(&nsb, SCARG(uap, ub), sizeof(nsb));
	return (error);
}

/*
 * Return status information about a file descriptor.
 */
/* ARGSUSED */
int
netbsd_sys___fstat13(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct netbsd_sys___fstat13_args /* {
		syscallarg(int) fd;
		syscallarg(struct netbsd_stat *) ub;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct netbsd_stat nsb;
	struct stat sb;
	int error;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	error = (*fp->f_ops->fo_stat)(fp, &sb, p);
	if (error)
		return (error);
	openbsd_to_netbsd_stat(&sb, &nsb);
	error = copyout(&nsb, SCARG(uap, ub), sizeof(nsb));
	return (error);
}

int
compat_43_netbsd_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_netbsd_sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ostat *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	NETBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return compat_43_sys_stat(p, uap, retval);
}

int
compat_43_netbsd_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_netbsd_sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ostat *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	NETBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return compat_43_sys_lstat(p, uap, retval);
}

int
netbsd_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct netbsd_sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	NETBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_stat(p, uap, retval);
}

int
netbsd_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct netbsd_sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	NETBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_lstat(p, uap, retval);
}
