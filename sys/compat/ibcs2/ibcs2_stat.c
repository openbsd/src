/*	$OpenBSD: ibcs2_stat.c,v 1.10 2002/08/02 18:06:25 millert Exp $	*/
/*	$NetBSD: ibcs2_stat.c,v 1.5 1996/05/03 17:05:32 christos Exp $	*/

/*
 * Copyright (c) 1995 Scott Bartram
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include <compat/ibcs2/ibcs2_types.h>
#include <compat/ibcs2/ibcs2_fcntl.h>
#include <compat/ibcs2/ibcs2_signal.h>
#include <compat/ibcs2/ibcs2_stat.h>
#include <compat/ibcs2/ibcs2_statfs.h>
#include <compat/ibcs2/ibcs2_syscallargs.h>
#include <compat/ibcs2/ibcs2_ustat.h>
#include <compat/ibcs2/ibcs2_util.h>
#include <compat/ibcs2/ibcs2_utsname.h>

static void bsd_stat2ibcs_stat(struct ostat *, struct ibcs2_stat *);
static int cvt_statfs(struct statfs *, caddr_t, int);

static void
bsd_stat2ibcs_stat(st, st4)
	struct ostat *st;
	struct ibcs2_stat *st4;
{
	bzero(st4, sizeof(*st4));
	st4->st_dev = (ibcs2_dev_t)st->st_dev;
	st4->st_ino = (ibcs2_ino_t)st->st_ino;
	st4->st_mode = (ibcs2_mode_t)st->st_mode;
	st4->st_nlink = (ibcs2_nlink_t)st->st_nlink;
	st4->st_uid = (ibcs2_uid_t)st->st_uid;
	st4->st_gid = (ibcs2_gid_t)st->st_gid;
	st4->st_rdev = (ibcs2_dev_t)st->st_rdev;
	st4->st_size = (ibcs2_off_t)st->st_size;
	st4->st_atim = (ibcs2_time_t)st->st_atime;
	st4->st_mtim = (ibcs2_time_t)st->st_mtime;
	st4->st_ctim = (ibcs2_time_t)st->st_ctime;
}

static int
cvt_statfs(sp, buf, len)
	struct statfs *sp;
	caddr_t buf;
	int len;
{
	struct ibcs2_statfs ssfs;

	if (len < 0)
		return (EINVAL);
	if (len > sizeof(ssfs))
		len = sizeof(ssfs);

	bzero(&ssfs, sizeof ssfs);
	ssfs.f_fstyp = 0;
	ssfs.f_bsize = sp->f_bsize;
	ssfs.f_frsize = 0;
	ssfs.f_blocks = sp->f_blocks;
	ssfs.f_bfree = sp->f_bfree;
	ssfs.f_files = sp->f_files;
	ssfs.f_ffree = sp->f_ffree;
	ssfs.f_fname[0] = 0;
	ssfs.f_fpack[0] = 0;
	return copyout((caddr_t)&ssfs, buf, len);
}	

int
ibcs2_sys_statfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_statfs_args /* {
		syscallarg(char *) path;
		syscallarg(struct ibcs2_statfs *) buf;
		syscallarg(int) len;
		syscallarg(int) fstype;
	} */ *uap = v;
	register struct mount *mp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;
	caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if ((error = VFS_STATFS(mp, sp, p)) != 0)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return cvt_statfs(sp, (caddr_t)SCARG(uap, buf), SCARG(uap, len));
}

int
ibcs2_sys_fstatfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_fstatfs_args /* {
		syscallarg(int) fd;
		syscallarg(struct ibcs2_statfs *) buf;
		syscallarg(int) len;
		syscallarg(int) fstype;
	} */ *uap = v;
	struct file *fp;
	struct mount *mp;
	register struct statfs *sp;
	int error;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	FREF(fp);
	error = VFS_STATFS(mp, sp, p);
	FRELE(fp);
	if (error)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return cvt_statfs(sp, (caddr_t)SCARG(uap, buf), SCARG(uap, len));
}

int
ibcs2_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ibcs2_stat *) st;
	} */ *uap = v;
	struct ostat st;
	struct ibcs2_stat ibcs2_st;
	struct compat_43_sys_stat_args cup;
	int error;
	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(st));
	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&cup, path) = SCARG(uap, path);

	if ((error = compat_43_sys_stat(p, &cup, retval)) != 0)
		return error;
	if ((error = copyin(SCARG(&cup, ub), &st, sizeof(st))) != 0)
		return error;
	bsd_stat2ibcs_stat(&st, &ibcs2_st);
	return copyout((caddr_t)&ibcs2_st, (caddr_t)SCARG(uap, st),
		       ibcs2_stat_len);
}

int
ibcs2_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ibcs2_stat *) st;
	} */ *uap = v;
	struct ostat st;
	struct ibcs2_stat ibcs2_st;
	struct compat_43_sys_lstat_args cup;
	int error;
	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(st));
	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&cup, path) = SCARG(uap, path);

	if ((error = compat_43_sys_lstat(p, &cup, retval)) != 0)
		return error;
	if ((error = copyin(SCARG(&cup, ub), &st, sizeof(st))) != 0)
		return error;
	bsd_stat2ibcs_stat(&st, &ibcs2_st);
	return copyout((caddr_t)&ibcs2_st, (caddr_t)SCARG(uap, st),
		       ibcs2_stat_len);
}

int
ibcs2_sys_fstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_fstat_args /* {
		syscallarg(int) fd;
		syscallarg(struct ibcs2_stat *) st;
	} */ *uap = v;
	struct ostat st;
	struct ibcs2_stat ibcs2_st;
	struct compat_43_sys_fstat_args cup;
	int error;
	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&cup, fd) = SCARG(uap, fd);
	SCARG(&cup, sb) = stackgap_alloc(&sg, sizeof(st));
	if ((error = compat_43_sys_fstat(p, &cup, retval)) != 0)
		return error;
	if ((error = copyin(SCARG(&cup, sb), &st, sizeof(st))) != 0)
		return error;
	bsd_stat2ibcs_stat(&st, &ibcs2_st);
	return copyout((caddr_t)&ibcs2_st, (caddr_t)SCARG(uap, st),
		       ibcs2_stat_len);
}

int
ibcs2_sys_utssys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_utssys_args /* {
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) flag;
	} */ *uap = v;

	switch (SCARG(uap, flag)) {
	case 0:			/* uname(2) */
	{
		struct ibcs2_utsname sut;
		extern char machine[];

		bzero(&sut, ibcs2_utsname_len);
		bcopy(ostype, sut.sysname, sizeof(sut.sysname) - 1);
		bcopy(hostname, sut.nodename, sizeof(sut.nodename));
		sut.nodename[sizeof(sut.nodename)-1] = '\0';
		bcopy(osrelease, sut.release, sizeof(sut.release) - 1);
		bcopy("1", sut.version, sizeof(sut.version) - 1);
		bcopy(machine, sut.machine, sizeof(sut.machine) - 1);

		return copyout((caddr_t)&sut, (caddr_t)SCARG(uap, a1),
			       ibcs2_utsname_len);
	}

	case 2:			/* ustat(2) */
	{
		return ENOSYS;	/* XXX - TODO */
	}

	default:
		return ENOSYS;
	}
}
