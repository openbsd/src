/*	$NetBSD: freebsd_file.c,v 1.2 1995/11/07 22:27:21 gwr Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
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
 *
 *	from: linux_file.c,v 1.3 1995/04/04 04:21:30 mycroft Exp
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

#include <sys/syscallargs.h>

#include <compat/freebsd/freebsd_syscallargs.h>
#include <compat/freebsd/freebsd_util.h>

#define	ARRAY_LENGTH(array)	(sizeof(array)/sizeof(array[0]))

const char freebsd_emul_path[] = "/emul/freebsd";

static char *
convert_from_freebsd_mount_type(type)
	int type;
{
	static char *netbsd_mount_type[] = {
		NULL,     /*  0 = MOUNT_NONE */
		"ffs",	  /*  1 = "Fast" Filesystem */
		"nfs",	  /*  2 = Network Filesystem */
		"mfs",	  /*  3 = Memory Filesystem */
		"msdos",  /*  4 = MSDOS Filesystem */
		"lfs",	  /*  5 = Log-based Filesystem */
		"lofs",	  /*  6 = Loopback filesystem */
		"fdesc",  /*  7 = File Descriptor Filesystem */
		"portal", /*  8 = Portal Filesystem */
		"null",	  /*  9 = Minimal Filesystem Layer */
		"umap",	  /* 10 = User/Group Identifier Remapping Filesystem */
		"kernfs", /* 11 = Kernel Information Filesystem */
		"procfs", /* 12 = /proc Filesystem */
		"afs",	  /* 13 = Andrew Filesystem */
		"cd9660", /* 14 = ISO9660 (aka CDROM) Filesystem */
		"union",  /* 15 = Union (translucent) Filesystem */
		NULL,     /* 16 = "devfs" - existing device Filesystem */
#if 0 /* These filesystems don't exist in FreeBSD */
		"adosfs", /* ?? = AmigaDOS Filesystem */
#endif
	};

	if (type < 0 || type >= ARRAY_LENGTH(netbsd_mount_type))
		return (NULL);
	return (netbsd_mount_type[type]);
}

int
freebsd_sys_mount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_mount_args /* {
		syscallarg(int) type;
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	int error;
	char *type, *s;
	caddr_t sg = stackgap_init(p->p_emul);
	struct sys_mount_args bma;

	if ((type = convert_from_freebsd_mount_type(SCARG(uap, type))) == NULL)
		return ENODEV;
	s = stackgap_alloc(&sg, MFSNAMELEN + 1);
	if (error = copyout(type, s, strlen(type) + 1))
		return error;
	SCARG(&bma, type) = s;
	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&bma, path) = SCARG(uap, path);
	SCARG(&bma, flags) = SCARG(uap, flags);
	SCARG(&bma, data) = SCARG(uap, data);
	return sys_mount(p, &bma, retval);
}

/*
 * The following syscalls are only here because of the alternate path check.
 */

/* XXX - UNIX domain: int freebsd_sys_bind(int s, caddr_t name, int namelen); */
/* XXX - UNIX domain: int freebsd_sys_connect(int s, caddr_t name, int namelen); */


int
freebsd_sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	if (SCARG(uap, flags) & O_CREAT)
		FREEBSD_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	else
		FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_open(p, uap, retval);
}

int
compat_43_freebsd_sys_creat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_freebsd_sys_creat_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	caddr_t sg  = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	return compat_43_sys_creat(p, uap, retval);
}

int
freebsd_sys_link(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_link_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	FREEBSD_CHECK_ALT_CREAT(p, &sg, SCARG(uap, link));
	return sys_link(p, uap, retval);
}

int
freebsd_sys_unlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_unlink_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_unlink(p, uap, retval);
}

int
freebsd_sys_chdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_chdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_chdir(p, uap, retval);
}

int
freebsd_sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_mknod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
		syscallarg(int) dev;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	return sys_mknod(p, uap, retval);
}

int
freebsd_sys_chmod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_chmod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_chmod(p, uap, retval);
}

int
freebsd_sys_chown(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_chown_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_chown(p, uap, retval);
}

int
freebsd_sys_unmount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_unmount_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_unmount(p, uap, retval);
}

int
freebsd_sys_access(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_access_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_access(p, uap, retval);
}

int
freebsd_sys_chflags(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_chflags_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_chflags(p, uap, retval);
}

int
compat_43_freebsd_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_freebsd_sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ostat *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return compat_43_sys_stat(p, uap, retval);
}

int
compat_43_freebsd_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_freebsd_sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ostat *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return compat_43_sys_lstat(p, uap, retval);
}

int
freebsd_sys_revoke(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_revoke_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_revoke(p, uap, retval);
}

int
freebsd_sys_symlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_symlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	FREEBSD_CHECK_ALT_CREAT(p, &sg, SCARG(uap, link));
	return sys_symlink(p, uap, retval);
}

int
freebsd_sys_readlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_readlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) buf;
		syscallarg(int) count;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_readlink(p, uap, retval);
}

int
freebsd_sys_execve(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_execve_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argp;
		syscallarg(char **) envp;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_execve(p, uap, retval);
}

int
freebsd_sys_chroot(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_chroot_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_chroot(p, uap, retval);
}

int
freebsd_sys_rename(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_rename_args /* {
		syscallarg(char *) from;
		syscallarg(char *) to;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, from));
	FREEBSD_CHECK_ALT_CREAT(p, &sg, SCARG(uap, to));
	return sys_rename(p, uap, retval);
}

int
compat_43_freebsd_sys_truncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_freebsd_sys_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(long) length;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return compat_43_sys_truncate(p, uap, retval);
}

int
freebsd_sys_mkfifo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_mkfifo_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	return sys_mkfifo(p, uap, retval);
}

int
freebsd_sys_mkdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_mkdir_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	return sys_mkdir(p, uap, retval);
}

int
freebsd_sys_rmdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_rmdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_rmdir(p, uap, retval);
}

int
freebsd_sys_statfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct statfs *) buf;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_statfs(p, uap, retval);
}

#ifdef NFSCLIENT
int
freebsd_sys_getfh(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_getfh_args /* {
		syscallarg(char *) fname;
		syscallarg(fhandle_t *) fhp;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, fname));
	return sys_getfh(p, uap, retval);
}
#endif /* NFSCLIENT */

int
freebsd_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_stat(p, uap, retval);
}

int
freebsd_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_lstat(p, uap, retval);
}

int
freebsd_sys_pathconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_pathconf_args /* {
		syscallarg(char *) path;
		syscallarg(int) name;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_pathconf(p, uap, retval);
}

int
freebsd_sys_truncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_truncate(p, uap, retval);
}
