/*	$NetBSD: ultrix_misc.c,v 1.16.2.1 1995/10/18 06:46:14 jonathan Exp $	*/

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
 * SunOS compatibility module.
 *
 * SunOS system calls that are implemented differently in BSD are
 * handled here.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/dir.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/unistd.h>
#include <sys/syscallargs.h>

#include <compat/ultrix/ultrix_syscall.h>
#include <compat/ultrix/ultrix_syscallargs.h>

#include <netinet/in.h>

#include <miscfs/specfs/specdev.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfs.h>

#include <vm/vm.h>

extern struct sysent ultrix_sysent[];
extern char *ultrix_syscallnames[];
extern void cpu_exec_ecoff_setregs __P((struct proc *, struct exec_package *,
					u_long, register_t *));
struct emul emul_ultrix = {
	"ultrix",
	NULL,
	sendsig,
	ULTRIX_SYS_syscall,
	ULTRIX_SYS_MAXSYSCALL,
	ultrix_sysent,
	ultrix_syscallnames,
	0,
	copyargs,
	cpu_exec_ecoff_setregs,
	0,
	0,
};

#define GSI_PROG_ENV 1

ultrix_sys_getsysinfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_getsysinfo_args *uap = v;
	static short progenv = 0;

	switch (SCARG(uap, op)) {
		/* operations implemented: */
	case GSI_PROG_ENV:
		if (SCARG(uap, nbytes) < sizeof(short))
			return EINVAL;
		*retval = 1;
		return (copyout(&progenv, SCARG(uap, buffer), sizeof(short)));
	default:
		*retval = 0; /* info unavail */
		return 0;
	}
}

ultrix_sys_setsysinfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_setsysinfo_args *uap = v;
	*retval = 0;
	return 0;
}

ultrix_sys_waitpid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_waitpid_args *uap = v;
	struct sys_wait4_args ua;

	SCARG(&ua, pid) = SCARG(uap, pid);
	SCARG(&ua, status) = SCARG(uap, status);
	SCARG(&ua, options) = SCARG(uap, options);
	SCARG(&ua, rusage) = 0;

	return (sys_wait4(p, &ua, retval));
}

ultrix_sys_wait3(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_wait3_args *uap = v;
	struct sys_wait4_args ua;

	SCARG(&ua, pid) = -1;
	SCARG(&ua, status) = SCARG(uap, status);
	SCARG(&ua, options) = SCARG(uap, options);
	SCARG(&ua, rusage) = SCARG(uap, rusage);

	return (sys_wait4(p, &ua, retval));
}

ultrix_sys_execv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_execv_args *uap = v;
	struct sys_execve_args ouap;

	SCARG(&ouap, path) = SCARG(uap, path);
	SCARG(&ouap, argp) = SCARG(uap, argp);
	SCARG(&ouap, envp) = NULL;

	return (sys_execve(p, &ouap, retval));
}

#if defined(NFSCLIENT)
async_daemon(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_nfssvc_args ouap;

	SCARG(&ouap, flag) = NFSSVC_BIOD;
	SCARG(&ouap, argp) = NULL;

	return (sys_nfssvc(p, &ouap, retval));
}
#endif /* NFSCLIENT */

ultrix_sys_sigpending(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_sigpending_args *uap = v;
	int mask = p->p_siglist & p->p_sigmask;

	return (copyout((caddr_t)&mask, (caddr_t)SCARG(uap, mask), sizeof(int)));
}

#if 0
/* XXX: Temporary until sys/dir.h, include/dirent.h and sys/dirent.h are fixed */
struct dirent {
	u_long	d_fileno;		/* file number of entry */
	u_short	d_reclen;		/* length of this record */
	u_short	d_namlen;		/* length of string in d_name */
	char	d_name[255 + 1];	/* name must be no longer than this */
};
#endif

#define	SUN__MAP_NEW	0x80000000	/* if not, old mmap & cannot handle */

ultrix_sys_mmap(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct ultrix_sys_mmap_args *uap = v;
	struct sys_mmap_args ouap;
	register struct filedesc *fdp;
	register struct file *fp;
	register struct vnode *vp;

	/*
	 * Verify the arguments.
	 */
	if (SCARG(uap, prot) & ~(PROT_READ|PROT_WRITE|PROT_EXEC))
		return (EINVAL);			/* XXX still needed? */

	if ((SCARG(uap, flags) & SUN__MAP_NEW) == 0)
		return (EINVAL);

	SCARG(&ouap, flags) = SCARG(uap, flags) & ~SUN__MAP_NEW;
	SCARG(&ouap, addr) = SCARG(uap, addr);

	if ((SCARG(&ouap, flags) & MAP_FIXED) == 0 &&
	    SCARG(&ouap, addr) != 0 &&
	    SCARG(&ouap, addr) < (caddr_t)round_page(p->p_vmspace->vm_daddr+MAXDSIZ))
		SCARG(&ouap, addr) = (caddr_t)round_page(p->p_vmspace->vm_daddr+MAXDSIZ);

	SCARG(&ouap, len) = SCARG(uap, len);
	SCARG(&ouap, prot) = SCARG(uap, prot);
	SCARG(&ouap, fd) = SCARG(uap, fd);
	SCARG(&ouap, pos) = SCARG(uap, pos);

	/*
	 * Special case: if fd refers to /dev/zero, map as MAP_ANON.  (XXX)
	 */
	fdp = p->p_fd;
	if ((unsigned)SCARG(&ouap, fd) < fdp->fd_nfiles &&		/*XXX*/
	    (fp = fdp->fd_ofiles[SCARG(&ouap, fd)]) != NULL &&		/*XXX*/
	    fp->f_type == DTYPE_VNODE &&				/*XXX*/
	    (vp = (struct vnode *)fp->f_data)->v_type == VCHR &&	/*XXX*/
	    iszerodev(vp->v_rdev)) {					/*XXX*/
		SCARG(&ouap, flags) |= MAP_ANON;
		SCARG(&ouap, fd) = -1;
	}

	return (sys_mmap(p, &ouap, retval));
}

ultrix_sys_setsockopt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct ultrix_sys_setsockopt_args *uap = v;
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	if (error = getsock(p->p_fd, SCARG(uap, s), &fp))
		return (error);
#define	SO_DONTLINGER (~SO_LINGER)
	if (SCARG(uap, name) == SO_DONTLINGER) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (m == NULL)
			return (ENOBUFS);
		mtod(m, struct linger *)->l_onoff = 0;
		m->m_len = sizeof(struct linger);
		return (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
		    SO_LINGER, m));
	}
	if (SCARG(uap, valsize) > MLEN)
		return (EINVAL);
	if (SCARG(uap, val)) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (m == NULL)
			return (ENOBUFS);
		if (error = copyin(SCARG(uap, val), mtod(m, caddr_t),
		    (u_int)SCARG(uap, valsize))) {
			(void) m_free(m);
			return (error);
		}
		m->m_len = SCARG(uap, valsize);
	}
	return (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
	    SCARG(uap, name), m));
}

struct ultrix_utsname {
	char    sysname[9];
	char    nodename[9];
	char    nodeext[65-9];
	char    release[9];
	char    version[9];
	char    machine[9];
};

ultrix_sys_uname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_uname_args *uap = v;
	struct ultrix_utsname sut;
	extern char ostype[], machine[], osrelease[];

	bzero(&sut, sizeof(sut));

	bcopy(ostype, sut.sysname, sizeof(sut.sysname) - 1);
	bcopy(hostname, sut.nodename, sizeof(sut.nodename));
	sut.nodename[sizeof(sut.nodename)-1] = '\0';
	bcopy(osrelease, sut.release, sizeof(sut.release) - 1);
	bcopy("1", sut.version, sizeof(sut.version) - 1);
	bcopy(machine, sut.machine, sizeof(sut.machine) - 1);

	return copyout((caddr_t)&sut, (caddr_t)SCARG(uap, name),
	    sizeof(struct ultrix_utsname));
}

int
ultrix_sys_setpgrp(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_setpgrp_args *uap = v;

	/*
	 * difference to our setpgid call is to include backwards
	 * compatibility to pre-setsid() binaries. Do setsid()
	 * instead of setpgid() in those cases where the process
	 * tries to create a new session the old way.
	 */
	if (!SCARG(uap, pgid) &&
	    (!SCARG(uap, pid) || SCARG(uap, pid) == p->p_pid))
		return sys_setsid(p, uap, retval);
	else
		return sys_setpgid(p, uap, retval);
}

ultrix_sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_open_args *uap = v;
	int l, r;
	int noctty;
	int ret;
	
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

#if defined (NFSSERVER)
ultrix_sys_nfssvc(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_nfssvc_args *uap = v;
	struct emul *e = p->p_emul;
	struct sys_nfssvc_args outuap;
	struct sockaddr sa;
	int error;

#if 0
	bzero(&outuap, sizeof outuap);
	SCARG(&outuap, fd) = SCARG(uap, fd);
	SCARG(&outuap, mskval) = STACKGAPBASE;
	SCARG(&outuap, msklen) = sizeof sa;
	SCARG(&outuap, mtchval) = outuap.mskval + sizeof sa;
	SCARG(&outuap, mtchlen) = sizeof sa;

	bzero(&sa, sizeof sa);
	if (error = copyout(&sa, SCARG(&outuap, mskval), SCARG(&outuap, msklen)))
		return (error);
	if (error = copyout(&sa, SCARG(&outuap, mtchval), SCARG(&outuap, mtchlen)))
		return (error);

	return nfssvc(p, &outuap, retval);
#else
	return (ENOSYS);
#endif
}
#endif /* NFSSERVER */

struct ultrix_ustat {
	daddr_t	f_tfree;	/* total free */
	ino_t	f_tinode;	/* total inodes free */
	char	f_fname[6];	/* filsys name */
	char	f_fpack[6];	/* filsys pack name */
};

ultrix_sys_ustat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_ustat_args *uap = v;
	struct ultrix_ustat us;
	int error;

	bzero(&us, sizeof us);

	/*
	 * XXX: should set f_tfree and f_tinode at least
	 * How do we translate dev -> fstat? (and then to ultrix_ustat)
	 */

	if (error = copyout(&us, SCARG(uap, buf), sizeof us))
		return (error);
	return 0;
}

ultrix_sys_quotactl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_quotactl_args *uap = v;

	return EINVAL;
}

ultrix_sys_vhangup(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return 0;
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

static
sunstatfs(sp, buf)
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

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return sunstatfs(sp, (caddr_t)SCARG(uap, buf));
}

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
	return sunstatfs(sp, (caddr_t)SCARG(uap, buf));
}

ultrix_sys_exportfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_exportfs_args *uap = v;

	/*
	 * XXX: should perhaps translate into a mount(2)
	 * with MOUNT_EXPORT?
	 */
	return 0;
}

ultrix_sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_mknod_args *uap = v;

	if (S_ISFIFO(SCARG(uap, mode)))
		return sys_mkfifo(p, uap, retval);

	return sys_mknod(p, (struct sys_mknod_args *)uap, retval);
}

int
ultrix_sys_sigcleanup(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_sigcleanup_args *uap = v;

	return sys_sigreturn(p, (struct sys_sigreturn_args *)uap, retval);
}
