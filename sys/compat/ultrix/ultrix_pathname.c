/*	$NetBSD: ultrix_pathname.c,v 1.1 1996/01/07 13:38:52 jonathan Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *
 *	@(#)sun_misc.c	8.1 (Berkeley) 6/18/93
 *
 * from: Header: sun_misc.c,v 1.16 93/04/07 02:46:27 torek Exp 
 */


/*
 * Ultrix emulation filesystem-namespace compatibility module.
 *
 * Ultrix system calls that examine the filesysten namespace
 * are implemented here.  Each system call has a wrapper that
 * first checks if the given file exists at a special `emulation'
 * pathname: the given path, prefixex with '/emul/ultrix', and
 * if that pathname exists, it is used instead of the providd pathname.
 *
 * Used to locate OS-specific files (shared libraries, config files,
 * etc) used by emul processes at their `normal' pathnames, without
 * polluting, or conflicting with, the native filesysten namespace.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/syscallargs.h>

#include <compat/ultrix/ultrix_syscallargs.h>
#include <compat/ultrix/ultrix_util.h>

const char ultrix_emul_path[] = "/emul/ultrix";

int
ultrix_sys_creat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_creat_args *uap = v;
	struct sys_open_args ouap;

	caddr_t sg = stackgap_init(p->p_emul);
	ULTRIX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ouap, path) = SCARG(uap, path);
	SCARG(&ouap, flags) = O_WRONLY | O_CREAT | O_TRUNC;
	SCARG(&ouap, mode) = SCARG(uap, mode);

	return (sys_open(p, &ouap, retval));
}


int
ultrix_sys_access(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_access_args *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);
	ULTRIX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return (sys_access(p, uap, retval));
}

int
ultrix_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_stat_args *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);
	ULTRIX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return (compat_43_sys_stat(p, uap, retval));
}

int
ultrix_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_lstat_args *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);
	ULTRIX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return (compat_43_sys_lstat(p, uap, retval));
}

int
ultrix_sys_execv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_execv_args *uap = v;
	struct sys_execve_args ouap;

	caddr_t sg = stackgap_init(p->p_emul);
	ULTRIX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ouap, path) = SCARG(uap, path);
	SCARG(&ouap, argp) = SCARG(uap, argp);
	SCARG(&ouap, envp) = NULL;

	return (sys_execve(p, &ouap, retval));
}

int
ultrix_sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_open_args *uap = v;
	int l, r;
	int noctty;
	int ret;
	
	caddr_t sg = stackgap_init(p->p_emul);
	ULTRIX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	/* convert open flags into NetBSD flags */
	l = SCARG(uap, flags);
	noctty = l & 0x8000;
	r =	(l & (0x0001 | 0x0002 | 0x0008 | 0x0040 | 0x0200 | 0x0400 | 0x0800));
	r |=	((l & (0x0004 | 0x1000 | 0x4000)) ? O_NONBLOCK : 0);
	r |=	((l & 0x0080) ? O_SHLOCK : 0);
	r |=	((l & 0x0100) ? O_EXLOCK : 0);
	r |=	((l & 0x2000) ? O_FSYNC : 0);

	SCARG(uap, flags) = r;
	ret = sys_open(p, (struct sys_open_args *)uap, retval);

	if (!ret && !noctty && SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
		struct filedesc *fdp = p->p_fd;
		struct file *fp = fdp->fd_ofiles[*retval];

		/* ignore any error, just give it a try */
		if (fp->f_type == DTYPE_VNODE)
			(fp->f_ops->fo_ioctl)(fp, TIOCSCTTY, (caddr_t)0, p);
	}
	return ret;
}


struct ultrix_statfs {
	long	f_type;		/* type of info, zero for now */
	long	f_bsize;	/* fundamental file system block size */
	long	f_blocks;	/* total blocks in file system */
	long	f_bfree;	/* free blocks */
	long	f_bavail;	/* free blocks available to non-super-user */
	long	f_files;	/* total file nodes in file system */
	long	f_ffree;	/* free file nodes in fs */
	fsid_t	f_fsid;		/* file system id */
	long	f_spare[7];	/* spare for later */
};

/*
 * Custruct ultrix statfs result from native. 
 * XXX should this be the same as returned by Ultrix getmnt(2)?
 * XXX Ultrix predates DEV_BSIZE.  Is  conversion of disk space from 1k
 *  block units to DEV_BSIZE necessary? 
 */
static int
ultrixstatfs(sp, buf)
	struct statfs *sp;
	caddr_t buf;
{
	struct ultrix_statfs ssfs;

	bzero(&ssfs, sizeof ssfs);
	ssfs.f_type = 0;
	ssfs.f_bsize = sp->f_bsize;
	ssfs.f_blocks = sp->f_blocks;
	ssfs.f_bfree = sp->f_bfree;
	ssfs.f_bavail = sp->f_bavail;
	ssfs.f_files = sp->f_files;
	ssfs.f_ffree = sp->f_ffree;
	ssfs.f_fsid = sp->f_fsid;
	return copyout((caddr_t)&ssfs, buf, sizeof ssfs);
}


int
ultrix_sys_statfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_statfs_args *uap = v;
	register struct mount *mp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;

	caddr_t sg = stackgap_init(p->p_emul);
	ULTRIX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return ultrixstatfs(sp, (caddr_t)SCARG(uap, buf));
}

/*
 * sys_fstatfs() takes an fd, not a path, and so needs no emul
 * pathname processing;  but it's similar enough to sys_statfs() that
 * it goes here anyway.
 */
int
ultrix_sys_fstatfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_fstatfs_args *uap = v;
	struct file *fp;
	struct mount *mp;
	register struct statfs *sp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return ultrixstatfs(sp, (caddr_t)SCARG(uap, buf));
}

int
ultrix_sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_mknod_args *uap = v;

	caddr_t sg = stackgap_init(p->p_emul);
	ULTRIX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	if (S_ISFIFO(SCARG(uap, mode)))
		return sys_mkfifo(p, uap, retval);

	return sys_mknod(p, (struct sys_mknod_args *)uap, retval);
}
