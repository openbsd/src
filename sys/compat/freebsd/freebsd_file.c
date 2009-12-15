/*	$OpenBSD: freebsd_file.c,v 1.26 2009/12/15 20:26:21 jasper Exp $	*/
/*	$NetBSD: freebsd_file.c,v 1.3 1996/05/03 17:03:09 christos Exp $	*/

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
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>

#include <sys/syscallargs.h>

#include <compat/freebsd/freebsd_signal.h>
#include <compat/freebsd/freebsd_syscallargs.h>
#include <compat/freebsd/freebsd_util.h>

const char freebsd_emul_path[] = "/emul/freebsd";

static char * convert_from_freebsd_mount_type(int);
void statfs_to_freebsd_statfs(struct proc *, struct mount *, struct statfs *, struct freebsd_statfs *);

struct freebsd_statfs {
	long	f_spare2;		/* placeholder */
	long	f_bsize;		/* fundamental file system block size */
	long	f_iosize;		/* optimal transfer block size */
	long	f_blocks;		/* total data blocks in file system */
	long	f_bfree;		/* free blocks in fs */
	long	f_bavail;		/* free blocks avail to non-superuser */
	long	f_files;		/* total file nodes in file system */
	long	f_ffree;		/* free file nodes in fs */
	fsid_t	f_fsid;			/* file system id */
	uid_t	f_owner;		/* user that mounted the filesystem */
	int	f_type;			/* type of filesystem */
	int	f_flags;		/* copy of mount exported flags */
	long    f_syncwrites;		/* count of sync writes since mount */
	long    f_asyncwrites;		/* count of async writes since mount */
	char	f_fstypename[MFSNAMELEN]; /* fs type name */
	char	f_mntonname[MNAMELEN];	/* directory on which mounted */
	char	f_mntfromname[MNAMELEN];/* mounted filesystem */
};

static char *
convert_from_freebsd_mount_type(type)
	int type;
{
	static char *freebsd_mount_type[] = {
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

	if (type < 0 || type >= nitems(freebsd_mount_type))
		return (NULL);
	return (freebsd_mount_type[type]);
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
	if ((error = copyout(type, s, strlen(type) + 1)) != 0)
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
		syscallarg(struct stat43 *) ub;
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
		syscallarg(struct stat43 *) ub;
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
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = SCARG(uap, envp);

	return sys_execve(p, &ap, retval);
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

/*
 * Convert struct statfs -> struct freebsd_statfs
 */
void
statfs_to_freebsd_statfs(p, mp, sp, fsp)
	struct proc *p;
	struct mount *mp;
	struct statfs *sp;
	struct freebsd_statfs *fsp;
{
	fsp->f_bsize = sp->f_bsize;
	fsp->f_iosize = sp->f_iosize;
	fsp->f_blocks = sp->f_blocks;
	fsp->f_bfree = sp->f_bfree;
	fsp->f_bavail = sp->f_bavail;
	fsp->f_files = sp->f_files;
	fsp->f_ffree = sp->f_ffree;
	/* Don't let non-root see filesystem id (for NFS security) */
	if (suser(p, 0))
		fsp->f_fsid.val[0] = fsp->f_fsid.val[1] = 0;
	else
		bcopy(&sp->f_fsid, &fsp->f_fsid, sizeof(fsp->f_fsid));
	fsp->f_owner = sp->f_owner;
	fsp->f_type = mp->mnt_vfc->vfc_typenum;
	fsp->f_flags = sp->f_flags;
	fsp->f_syncwrites = sp->f_syncwrites;
	fsp->f_asyncwrites = sp->f_asyncwrites;
	bcopy(sp->f_fstypename, fsp->f_fstypename, MFSNAMELEN);
	bcopy(sp->f_mntonname, fsp->f_mntonname, MNAMELEN);
	bcopy(sp->f_mntfromname, fsp->f_mntfromname, MNAMELEN);
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
int
freebsd_sys_statfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct freebsd_sys_statfs_args /* {
		syscallarg(char *) path;
		syscallarg(struct freebsd_statfs *) buf;
	} */ *uap = v;
	register struct mount *mp;
	register struct statfs *sp;
	struct freebsd_statfs fsb;
	int error;
	struct nameidata nd;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if ((error = VFS_STATFS(mp, sp, p)) != 0)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;

	statfs_to_freebsd_statfs(p, mp, sp, &fsb);
	return (copyout((caddr_t)&fsb, (caddr_t)SCARG(uap, buf), sizeof(fsb)));
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
int
freebsd_sys_fstatfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct freebsd_sys_fstatfs_args /* {
		syscallarg(int) fd;
		syscallarg(struct freebsd_statfs *) buf;
	} */ *uap = v;
	struct file *fp;
	struct mount *mp;
	register struct statfs *sp;
	struct freebsd_statfs fsb;
	int error;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	error = VFS_STATFS(mp, sp, p);
	FRELE(fp);
	if (error)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;

	statfs_to_freebsd_statfs(p, mp, sp, &fsb);
	return (copyout((caddr_t)&fsb, (caddr_t)SCARG(uap, buf), sizeof(fsb)));
}

/*
 * Get statistics on all filesystems.
 */
int
freebsd_sys_getfsstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct freebsd_sys_getfsstat_args /* {
		syscallarg(struct freebsd_statfs *) buf;
		syscallarg(long) bufsize;
		syscallarg(int) flags;
	} */ *uap = v;
	register struct mount *mp, *nmp;
	register struct statfs *sp;
	struct freebsd_statfs fsb;
	caddr_t sfsp;
	long count, maxcount;
	int error, flags = SCARG(uap, flags);

	maxcount = SCARG(uap, bufsize) / sizeof(struct freebsd_statfs);
	sfsp = (caddr_t)SCARG(uap, buf);
	count = 0;

	for (mp = CIRCLEQ_FIRST(&mountlist); mp != CIRCLEQ_END(&mountlist);
	    mp = nmp) {
		if (vfs_busy(mp, VB_READ|VB_NOWAIT)) {
			nmp = CIRCLEQ_NEXT(mp, mnt_list);
			continue;
		}
		if (sfsp && count < maxcount) {
			sp = &mp->mnt_stat;

			/* Refresh stats unless MNT_NOWAIT is specified */
			if (flags != MNT_NOWAIT &&
			    flags != MNT_LAZY &&
			    (flags == MNT_WAIT ||
			     flags == 0) &&
			    (error = VFS_STATFS(mp, sp, p))) {
				nmp = CIRCLEQ_NEXT(mp, mnt_list);
				vfs_unbusy(mp);
 				continue;
			}
			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;

			statfs_to_freebsd_statfs(p, mp, sp, &fsb);
			error = copyout((caddr_t)&fsb, sfsp, sizeof(fsb));
			if (error) {
				vfs_unbusy(mp);
				return (error);
			}
			sfsp += sizeof(fsb);
		}
		count++;
		nmp = CIRCLEQ_NEXT(mp, mnt_list);
		vfs_unbusy(mp);
	}

	if (sfsp && count > maxcount)
		*retval = maxcount;
	else
		*retval = count;

	return (0);
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
		syscallarg(struct stat35 *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return compat_35_sys_stat(p, uap, retval);
}

int
freebsd_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat35 *) ub;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	FREEBSD_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return compat_35_sys_lstat(p, uap, retval);
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

/*
 * Just pass on everything to our fcntl, except for F_[GS]ETOWN on pipes,
 * where we translate to SIOC[GS]PGRP.
 */
int
freebsd_sys_fcntl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_fcntl_args /* {
		syscallarg(int) fd;
		syscallarg(int) cmd;
		syscallarg(void *) arg;
	} */ *uap = v;
	int fd, cmd, error;
	struct filedesc *fdp;
	struct file *fp;

	fd = SCARG(uap, fd);
	cmd = SCARG(uap, cmd);

	switch (cmd) {
	case F_GETOWN:
	case F_SETOWN:
		/* Our pipes does not understand F_[GS]ETOWN.  */ 
		fdp = p->p_fd;
		if ((fp = fd_getfile(fdp, fd)) == NULL)
			return (EBADF);
		if (fp->f_type == DTYPE_PIPE) {
			FREF(fp);
			error = (*fp->f_ops->fo_ioctl)(fp,
			    cmd == F_GETOWN ? SIOCGPGRP : SIOCSPGRP,
			    (caddr_t)&SCARG(uap, arg), p);
			FRELE(fp);
			return (error);
		}
		break;
	}

	return (sys_fcntl(p, uap, retval));
}

