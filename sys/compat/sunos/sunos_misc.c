/*	$OpenBSD: sunos_misc.c,v 1.38 2002/07/25 22:18:27 nordin Exp $	*/
/*	$NetBSD: sunos_misc.c,v 1.65 1996/04/22 01:44:31 christos Exp $	*/

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
 *	@(#)sunos_misc.c	8.1 (Berkeley) 6/18/93
 *
 *	Header: sunos_misc.c,v 1.16 93/04/07 02:46:27 torek Exp 
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
#include <sys/proc.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/tty.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/unistd.h>
#include <sys/syscallargs.h>
#include <sys/conf.h>
#include <sys/socketvar.h>
#include <sys/times.h>

#include <compat/sunos/sunos.h>
#include <compat/sunos/sunos_syscallargs.h>
#include <compat/sunos/sunos_util.h>
#include <compat/sunos/sunos_dirent.h>

#include <compat/common/compat_dir.h>

#include <netinet/in.h>

#include <miscfs/specfs/specdev.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <uvm/uvm_extern.h>

#ifdef sun3
# include <machine/machdep.h>	/* for prototype of reboot2() */
#endif

static int sunstatfs(struct statfs *, caddr_t);

int
sunos_sys_wait4(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_wait4_args *uap = v;
	if (SCARG(uap, pid) == 0)
		SCARG(uap, pid) = WAIT_ANY;
	return (sys_wait4(p, uap, retval));
}

int
sunos_sys_creat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_creat_args *uap = v;
	struct sys_open_args ouap;

	caddr_t sg = stackgap_init(p->p_emul);
	SUNOS_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ouap, path) = SCARG(uap, path);
	SCARG(&ouap, flags) = O_WRONLY | O_CREAT | O_TRUNC;
	SCARG(&ouap, mode) = SCARG(uap, mode);

	return (sys_open(p, &ouap, retval));
}

int
sunos_sys_access(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_access_args *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);
	SUNOS_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return (sys_access(p, uap, retval));
}

int
sunos_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_stat_args *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);
	SUNOS_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return (compat_43_sys_stat(p, uap, retval));
}

int
sunos_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_lstat_args *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);
	SUNOS_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return (compat_43_sys_lstat(p, uap, retval));
}

int
sunos_sys_execve(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_execve_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argv;
		syscallarg(char **) envp;
        } */ *uap = v;
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	SUNOS_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = SCARG(uap, envp);

	return (sys_execve(p, &ap, retval));
}

int
sunos_sys_execv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_execv_args *uap = v;
	struct sys_execve_args ouap;

	caddr_t sg = stackgap_init(p->p_emul);
	SUNOS_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ouap, path) = SCARG(uap, path);
	SCARG(&ouap, argp) = SCARG(uap, argp);
	SCARG(&ouap, envp) = NULL;

	return (sys_execve(p, &ouap, retval));
}

int
sunos_sys_unmount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_unmount_args *uap = v;
	struct sys_unmount_args ouap;

	SCARG(&ouap, path) = SCARG(uap, path);
	SCARG(&ouap, flags) = 0;

	return (sys_unmount(p, &ouap, retval));
}

/*
 * Conversion table for SunOS NFS mount flags.
 */
static struct {
	int	sun_flg;
	int	bsd_flg;
} sunnfs_flgtab[] = {
	{ SUNNFS_SOFT,		NFSMNT_SOFT },
	{ SUNNFS_WSIZE,		NFSMNT_WSIZE },
	{ SUNNFS_RSIZE,		NFSMNT_RSIZE },
	{ SUNNFS_TIMEO,		NFSMNT_TIMEO },
	{ SUNNFS_RETRANS,	NFSMNT_RETRANS },
	{ SUNNFS_HOSTNAME,	0 },			/* Ignored */
	{ SUNNFS_INT,		NFSMNT_INT },
	{ SUNNFS_NOAC,		0 },			/* Ignored */
	{ SUNNFS_ACREGMIN,	0 },			/* Ignored */
	{ SUNNFS_ACREGMAX,	0 },			/* Ignored */
	{ SUNNFS_ACDIRMIN,	0 },			/* Ignored */
	{ SUNNFS_ACDIRMAX,	0 },			/* Ignored */
	{ SUNNFS_SECURE,	0 },			/* Ignored */
	{ SUNNFS_NOCTO,		0 },			/* Ignored */
	{ SUNNFS_POSIX,		0 }			/* Ignored */
};

int
sunos_sys_mount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_mount_args *uap = v;
	int oflags = SCARG(uap, flags), nflags, error;
	char fsname[MFSNAMELEN];
	caddr_t sg = stackgap_init(p->p_emul);

	if (oflags & (SUNM_NOSUB | SUNM_SYS5))
		return (EINVAL);
	if ((oflags & SUNM_NEWTYPE) == 0)
		return (EINVAL);
	nflags = 0;
	if (oflags & SUNM_RDONLY)
		nflags |= MNT_RDONLY;
	if (oflags & SUNM_NOSUID)
		nflags |= MNT_NOSUID;
	if (oflags & SUNM_REMOUNT)
		nflags |= MNT_UPDATE;
	SCARG(uap, flags) = nflags;

	error = copyinstr((caddr_t)SCARG(uap, type), fsname,
	    sizeof fsname, (size_t *)0);
	if (error)
		return (error);

	if (strncmp(fsname, "4.2", sizeof fsname) == 0) {
		SCARG(uap, type) = stackgap_alloc(&sg, sizeof("ffs"));
		error = copyout("ffs", SCARG(uap, type), sizeof("ffs"));
		if (error)
			return (error);
	} else if (strncmp(fsname, "nfs", sizeof fsname) == 0) {
		struct sunos_nfs_args sna;
		struct sockaddr_in sain;
		struct nfs_args na;
		struct sockaddr sa;
		int n;

		error = copyin(SCARG(uap, data), &sna, sizeof sna);
		if (error)
			return (error);
		error = copyin(sna.addr, &sain, sizeof sain);
		if (error)
			return (error);
		bcopy(&sain, &sa, sizeof sa);
		sa.sa_len = sizeof(sain);
		SCARG(uap, data) = stackgap_alloc(&sg, sizeof(na));
		na.version = NFS_ARGSVERSION;
		na.addr = stackgap_alloc(&sg, sizeof(struct sockaddr));
		na.addrlen = sizeof(struct sockaddr);
		na.sotype = SOCK_DGRAM;
		na.proto = IPPROTO_UDP;
		na.fh = (void *)sna.fh;
		na.fhsize = NFSX_V2FH;
		na.flags = 0;
		n = sizeof(sunnfs_flgtab) / sizeof(sunnfs_flgtab[0]);
		while (--n >= 0)
			if (sna.flags & sunnfs_flgtab[n].sun_flg)
				na.flags |= sunnfs_flgtab[n].bsd_flg;
		na.wsize = sna.wsize;
		na.rsize = sna.rsize;
		if (na.flags & NFSMNT_RSIZE) {
			na.flags |= NFSMNT_READDIRSIZE;
			na.readdirsize = na.rsize;
		}
		na.timeo = sna.timeo;
		na.retrans = sna.retrans;
		na.hostname = sna.hostname;

		error = copyout(&sa, na.addr, sizeof sa);
		if (error)
			return (error);
		error = copyout(&na, SCARG(uap, data), sizeof na);
		if (error)
			return (error);
	}
	return (sys_mount(p, (struct sys_mount_args *)uap, retval));
}

#if defined(NFSCLIENT)
int
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

int
sunos_sys_sigpending(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_sigpending_args *uap = v;
	int mask = p->p_siglist & p->p_sigmask;

	return (copyout((caddr_t)&mask, (caddr_t)SCARG(uap, mask), sizeof(int)));
}

/*
 * Read Sun-style directory entries.  We suck them into kernel space so
 * that they can be massaged before being copied out to user code.  Like
 * SunOS, we squish out `empty' entries.
 *
 * This is quite ugly, but what do you expect from compatibility code?
 */
int sunos_readdir_callback(void *, struct dirent *, off_t);

struct sunos_readdir_callback_args {
	caddr_t outp;
	int     resid;
};

int
sunos_readdir_callback(arg, bdp, cookie)
	void *arg;
	struct dirent *bdp;
	off_t cookie;
{
	struct sunos_dirent idb;
	struct sunos_readdir_callback_args *cb = arg; 
	int sunos_reclen;
	int error;

	sunos_reclen = SUNOS_RECLEN(&idb, bdp->d_namlen);
	if (cb->resid < sunos_reclen)
		return (ENOMEM);
	
	idb.d_fileno = bdp->d_fileno;
	idb.d_off = cookie;
	idb.d_reclen = sunos_reclen;
	idb.d_namlen = bdp->d_namlen;
	strlcpy(idb.d_name, bdp->d_name, SUNOS_MAXNAMLEN+1);

	if ((error = copyout((caddr_t)&idb, cb->outp, sunos_reclen)))
		return (error);

	cb->outp += sunos_reclen;
	cb->resid -= sunos_reclen;

	return (0);
}

int
sunos_sys_getdents(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_getdents_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(int) nbytes;
	} */ *uap = v;
	struct vnode *vp;
	struct file *fp;
	int error;
	struct sunos_readdir_callback_args args;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);

	vp = (struct vnode *)fp->f_data;
	/* SunOS returns ENOTDIR here, BSD would use EINVAL */
	if (vp->v_type != VDIR)
		return (ENOTDIR);

	args.resid = SCARG(uap, nbytes);
	args.outp = (caddr_t)SCARG(uap, buf);

	FREF(fp);
	error = readdir_with_callback(fp, &fp->f_offset, args.resid,
	    sunos_readdir_callback, &args);
	FRELE(fp);
	if (error)
		return (error);

	*retval = SCARG(uap, nbytes) - args.resid;

	return (0);
}


#define	SUNOS__MAP_NEW	0x80000000	/* if not, old mmap & cannot handle */

int
sunos_sys_mmap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_mmap_args *uap = v;
	struct sys_mmap_args ouap;

	/*
	 * Verify the arguments.
	 */
	if (SCARG(uap, prot) & ~(PROT_READ|PROT_WRITE|PROT_EXEC))
		return (EINVAL);			/* XXX still needed? */

	if ((SCARG(uap, flags) & SUNOS__MAP_NEW) == 0)
		return (EINVAL);

	SCARG(&ouap, flags) = SCARG(uap, flags) & ~SUNOS__MAP_NEW;
	SCARG(&ouap, addr) = SCARG(uap, addr);

	if ((SCARG(&ouap, flags) & MAP_FIXED) == 0 &&
	    SCARG(&ouap, addr) != 0 &&
	    SCARG(&ouap, addr) < (void *)round_page((vaddr_t)p->p_vmspace->vm_daddr+MAXDSIZ))
		SCARG(&ouap, addr) = (void *)round_page((vaddr_t)p->p_vmspace->vm_daddr+MAXDSIZ);

	SCARG(&ouap, len) = SCARG(uap, len);
	SCARG(&ouap, prot) = SCARG(uap, prot);
	SCARG(&ouap, fd) = SCARG(uap, fd);
	SCARG(&ouap, pos) = SCARG(uap, pos);

	return (sys_mmap(p, &ouap, retval));
}

#define	MC_SYNC		1
#define	MC_LOCK		2
#define	MC_UNLOCK	3
#define	MC_ADVISE	4
#define	MC_LOCKAS	5
#define	MC_UNLOCKAS	6

int
sunos_sys_mctl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sunos_sys_mctl_args *uap = v;

	switch (SCARG(uap, func)) {
	case MC_ADVISE:		/* ignore for now */
		return (0);
	case MC_SYNC:		/* translate to msync */
		return (sys_msync(p, uap, retval));
	default:
		return (EINVAL);
	}
}

int
sunos_sys_setsockopt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_setsockopt_args *uap = v;
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	FREF(fp);
#define	SO_DONTLINGER (~SO_LINGER)
	if (SCARG(uap, name) == SO_DONTLINGER) {
		m = m_get(M_WAIT, MT_SOOPTS);
		mtod(m, struct linger *)->l_onoff = 0;
		m->m_len = sizeof(struct linger);
		error = (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
		    SO_LINGER, m));
		FRELE(fp);
		return (error);
	}
	if (SCARG(uap, level) == IPPROTO_IP) {
#define		SUNOS_IP_MULTICAST_IF		2
#define		SUNOS_IP_MULTICAST_TTL		3
#define		SUNOS_IP_MULTICAST_LOOP		4
#define		SUNOS_IP_ADD_MEMBERSHIP		5
#define		SUNOS_IP_DROP_MEMBERSHIP	6
		static int ipoptxlat[] = {
			IP_MULTICAST_IF,
			IP_MULTICAST_TTL,
			IP_MULTICAST_LOOP,
			IP_ADD_MEMBERSHIP,
			IP_DROP_MEMBERSHIP
		};
		if (SCARG(uap, name) >= SUNOS_IP_MULTICAST_IF &&
		    SCARG(uap, name) <= SUNOS_IP_DROP_MEMBERSHIP) {
			SCARG(uap, name) =
			    ipoptxlat[SCARG(uap, name) - SUNOS_IP_MULTICAST_IF];
		}
	}
	if (SCARG(uap, valsize) > MLEN) {
		FRELE(fp);
		return (EINVAL);
	}
	if (SCARG(uap, val)) {
		m = m_get(M_WAIT, MT_SOOPTS);
		error = copyin(SCARG(uap, val), mtod(m, caddr_t),
		    (u_int)SCARG(uap, valsize));
		if (error) {
			FRELE(fp);
			(void) m_free(m);
			return (error);
		}
		m->m_len = SCARG(uap, valsize);
	}
	error = (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
	    SCARG(uap, name), m));
	FRELE(fp);
	return (error);
}

int
sunos_sys_fchroot(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sunos_sys_fchroot_args *uap = v;
	register struct filedesc *fdp = p->p_fd;
	register struct vnode *vp;
	struct file *fp;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	if ((error = getvnode(fdp, SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = (struct vnode *)fp->f_data;
	FREF(fp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	VOP_UNLOCK(vp, 0, p);
	if (error) {
		FRELE(fp);
		return (error);
	}
	VREF(vp);
	if (fdp->fd_rdir != NULL)
		vrele(fdp->fd_rdir);
	fdp->fd_rdir = vp;
	FRELE(fp);
	return (0);
}

/*
 * XXX: This needs cleaning up.
 */
int
sunos_sys_auditsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return 0;
}

int
sunos_sys_uname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_uname_args *uap = v;
	struct sunos_utsname sut;
	extern char machine[];

	bzero(&sut, sizeof(sut));

	bcopy(ostype, sut.sysname, sizeof(sut.sysname) - 1);
	bcopy(hostname, sut.nodename, sizeof(sut.nodename));
	sut.nodename[sizeof(sut.nodename)-1] = '\0';
	bcopy(osrelease, sut.release, sizeof(sut.release) - 1);
	bcopy("1", sut.version, sizeof(sut.version) - 1);
	bcopy(machine, sut.machine, sizeof(sut.machine) - 1);

	return copyout((caddr_t)&sut, (caddr_t)SCARG(uap, name),
	    sizeof(struct sunos_utsname));
}

int
sunos_sys_setpgrp(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_setpgrp_args *uap = v;

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

int
sunos_sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_open_args *uap = v;
	int l, r;
	int noctty;
	int ret;
	
	caddr_t sg = stackgap_init(p->p_emul);
	SUNOS_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	/* convert mode into NetBSD mode */
	l = SCARG(uap, flags);
	noctty = l & 0x8000;
	r =	(l & (0x0001 | 0x0002 | 0x0008 | 0x0040 | 0x0200 | 0x0400 | 0x0800));
	r |=	((l & (0x0004 | 0x1000 | 0x4000)) ? O_NONBLOCK : 0);
	r |=	((l & 0x0080) ? O_SHLOCK : 0);
	r |=	((l & 0x0100) ? O_EXLOCK : 0);
	r |=	((l & 0x2000) ? O_SYNC : 0);

	SCARG(uap, flags) = r;
	ret = sys_open(p, (struct sys_open_args *)uap, retval);

	if (!ret && !noctty && SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
		struct filedesc *fdp = p->p_fd;
		struct file *fp;

		if ((fp = fd_getfile(fdp, *retval)) == NULL)
			return (EBADF);
		FREF(fp);
		/* ignore any error, just give it a try */
		if (fp->f_type == DTYPE_VNODE)
			(fp->f_ops->fo_ioctl)(fp, TIOCSCTTY, (caddr_t)0, p);
		FRELE(fp);
	}
	return ret;
}

#if defined (NFSSERVER)
int
sunos_sys_nfssvc(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sunos_sys_nfssvc_args *uap = v;
	struct emul *e = p->p_emul;
	struct sys_nfssvc_args outuap;
	struct sockaddr sa;
	int error;

	bzero(&outuap, sizeof outuap);
	SCARG(&outuap, fd) = SCARG(uap, fd);
	SCARG(&outuap, mskval) = STACKGAPBASE;
	SCARG(&outuap, msklen) = sizeof sa;
	SCARG(&outuap, mtchval) = SCARG(&outuap, mskval) + sizeof sa;
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

int
sunos_sys_ustat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_ustat_args *uap = v;
	struct sunos_ustat us;
	int error;

	bzero(&us, sizeof us);

	/*
	 * XXX: should set f_tfree and f_tinode at least
	 * How do we translate dev -> fstat? (and then to sunos_ustat)
	 */

	if ((error = copyout(&us, SCARG(uap, buf), sizeof us)) != 0)
		return (error);
	return 0;
}

int
sunos_sys_quotactl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return EINVAL;
}

int
sunos_sys_vhangup(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct session *sp = p->p_session;

	if (sp->s_ttyvp == 0)
		return 0;

	if (sp->s_ttyp && sp->s_ttyp->t_session == sp && sp->s_ttyp->t_pgrp)
		pgsignal(sp->s_ttyp->t_pgrp, SIGHUP, 1);

	(void) ttywait(sp->s_ttyp);
	if (sp->s_ttyvp)
		VOP_REVOKE(sp->s_ttyvp, REVOKEALL);
	if (sp->s_ttyvp)
		vrele(sp->s_ttyvp);
	sp->s_ttyvp = NULL;

	return 0;
}

static int
sunstatfs(sp, buf)
	struct statfs *sp;
	caddr_t buf;
{
	struct sunos_statfs ssfs;

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
sunos_sys_statfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_statfs_args *uap = v;
	register struct mount *mp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;

	caddr_t sg = stackgap_init(p->p_emul);
	SUNOS_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if ((error = VFS_STATFS(mp, sp, p)) != 0)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return sunstatfs(sp, (caddr_t)SCARG(uap, buf));
}

int
sunos_sys_fstatfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_fstatfs_args *uap = v;
	struct file *fp;
	struct mount *mp;
	struct statfs *sp;
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
	return sunstatfs(sp, (caddr_t)SCARG(uap, buf));
}

int
sunos_sys_exportfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	/*
	 * XXX: should perhaps translate into a mount(2)
	 * with MOUNT_EXPORT?
	 */
	return 0;
}

int
sunos_sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_mknod_args *uap = v;

	caddr_t sg = stackgap_init(p->p_emul);
	SUNOS_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	if (S_ISFIFO(SCARG(uap, mode)))
		return sys_mkfifo(p, uap, retval);

	return sys_mknod(p, (struct sys_mknod_args *)uap, retval);
}

#define SUNOS_SC_ARG_MAX	1
#define SUNOS_SC_CHILD_MAX	2
#define SUNOS_SC_CLK_TCK	3
#define SUNOS_SC_NGROUPS_MAX	4
#define SUNOS_SC_OPEN_MAX	5
#define SUNOS_SC_JOB_CONTROL	6
#define SUNOS_SC_SAVED_IDS	7
#define SUNOS_SC_VERSION	8

int
sunos_sys_sysconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_sysconf_args *uap = v;
	extern int maxfiles;

	switch(SCARG(uap, name)) {
	case SUNOS_SC_ARG_MAX:
		*retval = ARG_MAX;
		break;
	case SUNOS_SC_CHILD_MAX:
		*retval = maxproc;
		break;
	case SUNOS_SC_CLK_TCK:
		*retval = 60;		/* should this be `hz', ie. 100? */
		break;
	case SUNOS_SC_NGROUPS_MAX:
		*retval = NGROUPS_MAX;
		break;
	case SUNOS_SC_OPEN_MAX:
		*retval = maxfiles;
		break;
	case SUNOS_SC_JOB_CONTROL:
		*retval = 1;
		break;
	case SUNOS_SC_SAVED_IDS:
#ifdef _POSIX_SAVED_IDS
		*retval = 1;
#else
		*retval = 0;
#endif
		break;
	case SUNOS_SC_VERSION:
		*retval = 198808;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

#define SUNOS_RLIMIT_NOFILE	6	/* Other RLIMIT_* are the same */
#define SUNOS_RLIM_NLIMITS	7

int
sunos_sys_getrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_getrlimit_args *uap = v;

	if (SCARG(uap, which) >= SUNOS_RLIM_NLIMITS)
		return EINVAL;

	if (SCARG(uap, which) == SUNOS_RLIMIT_NOFILE)
		SCARG(uap, which) = RLIMIT_NOFILE;

	return compat_43_sys_getrlimit(p, uap, retval);
}

int
sunos_sys_setrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_getrlimit_args *uap = v;

	if (SCARG(uap, which) >= SUNOS_RLIM_NLIMITS)
		return EINVAL;

	if (SCARG(uap, which) == SUNOS_RLIMIT_NOFILE)
		SCARG(uap, which) = RLIMIT_NOFILE;

	return compat_43_sys_setrlimit(p, uap, retval);
}

#ifdef PTRACE

static int sreq2breq[] = {
	PT_TRACE_ME,    PT_READ_I,      PT_READ_D,      -1,
	PT_WRITE_I,     PT_WRITE_D,     -1,             PT_CONTINUE,
	PT_KILL,        -1,             PT_ATTACH,      PT_DETACH,
	PT_GETREGS,     PT_SETREGS,     PT_GETFPREGS,   PT_SETFPREGS
};
static int nreqs = sizeof(sreq2breq) / sizeof(sreq2breq[0]);

int
sunos_sys_ptrace(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_ptrace_args *uap = v;
	struct sys_ptrace_args pa;
	int req;

	req = SCARG(uap, req);

	if (req < 0 || req >= nreqs)
		return (EINVAL);

	req = sreq2breq[req];
	if (req == -1)
		return (EINVAL);

	SCARG(&pa, req) = req;
	SCARG(&pa, pid) = (pid_t)SCARG(uap, pid);
	SCARG(&pa, addr) = (caddr_t)SCARG(uap, addr);
	SCARG(&pa, data) = SCARG(uap, data);

	return sys_ptrace(p, &pa, retval);
}

#endif	/* PTRACE */

/*
 * SunOS reboot system call (for compatibility).
 * Sun lets you pass in a boot string which the PROM
 * saves and provides to the next boot program.
 */
static struct sunos_howto_conv {
	int sun_howto;
	int bsd_howto;
} sunos_howto_conv[] = {
	{ 0x001,	RB_ASKNAME },
	{ 0x002,	RB_SINGLE },
	{ 0x004,	RB_NOSYNC },
	{ 0x008,	RB_HALT },
	{ 0x080,	RB_DUMP },
	{ 0x000,	0 },
};
#define	SUNOS_RB_STRING	0x200

int
sunos_sys_reboot(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_reboot_args *uap = v;
	struct sunos_howto_conv *convp;
	int error, bsd_howto, sun_howto;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	/*
	 * Convert howto bits to BSD format.
	 */
	sun_howto = SCARG(uap, howto);
	bsd_howto = 0;
	convp = sunos_howto_conv;
	while (convp->sun_howto) {
		if (sun_howto &  convp->sun_howto)
			bsd_howto |= convp->bsd_howto;
		convp++;
	}

#ifdef sun3
	/*
	 * Sun RB_STRING (Get user supplied bootstring.)
	 * If the machine supports passing a string to the
	 * next booted kernel, add the machine name above
	 * and provide a reboot2() function (see sun3).
	 */
	if (sun_howto & SUNOS_RB_STRING) {
		char bs[128];

		error = copyinstr(SCARG(uap, bootstr), bs, sizeof(bs), 0);
		if (error)
			return error;

		return (reboot2(bsd_howto, bs));
	}
#endif	/* sun3 */

	boot(bsd_howto);
	return 0;
}

/*
 * Generalized interface signal handler, 4.3-compatible.
 */
/* ARGSUSED */
int
sunos_sys_sigvec(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sunos_sys_sigvec_args /* {
		syscallarg(int) signum;
		syscallarg(struct sigvec *) nsv;
		syscallarg(struct sigvec *) osv;
	} */ *uap = v;
	struct sigvec vec;
	register struct sigacts *ps = p->p_sigacts;
	register struct sigvec *sv;
	register int signum;
	int bit, error;

	signum = SCARG(uap, signum);
	if (signum <= 0 || signum >= NSIG ||
	    signum == SIGKILL || signum == SIGSTOP)
		return (EINVAL);
	sv = &vec;
	if (SCARG(uap, osv)) {
		*(sig_t *)&sv->sv_handler = ps->ps_sigact[signum];
		sv->sv_mask = ps->ps_catchmask[signum];
		bit = sigmask(signum);
		sv->sv_flags = 0;
		if ((ps->ps_sigonstack & bit) != 0)
			sv->sv_flags |= SV_ONSTACK;
		if ((ps->ps_sigintr & bit) != 0)
			sv->sv_flags |= SV_INTERRUPT;
		if ((ps->ps_sigreset & bit) != 0)
			sv->sv_flags |= SA_RESETHAND;
		sv->sv_mask &= ~bit;
		error = copyout((caddr_t)sv, (caddr_t)SCARG(uap, osv),
		    sizeof (vec));
		if (error)
			return (error);
	}
	if (SCARG(uap, nsv)) {
		error = copyin((caddr_t)SCARG(uap, nsv), (caddr_t)sv,
		    sizeof (vec));
		if (error)
			return (error);
		/*
		 * SunOS uses the mask 0x0004 as SV_RESETHAND
		 * meaning: `reset to SIG_DFL on delivery'.
		 * We support only the bits in: 0xF
		 * (those bits are the same as ours)
		 */
		if (sv->sv_flags & ~0xF)
			return (EINVAL);
		/* SunOS binaries have a user-mode trampoline. */
		sv->sv_flags |= SA_USERTRAMP;
		/* Convert sigvec:SV_INTERRUPT to sigaction:SA_RESTART */
		sv->sv_flags ^= SA_RESTART;	/* same bit, inverted */
		setsigvec(p, signum, (struct sigaction *)sv);
	}
	return (0);
}

int
sunos_sys_ostime(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_ostime_args /* {
		syscallarg(int) time;
	} */ *uap = v;
	struct timeval tv;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	tv.tv_sec = SCARG(uap, time);
	tv.tv_usec = 0;
	error = settime(&tv);
	return (error);
}

/*
 * This code is partly stolen from src/lib/libc/gen/times.c
 * XXX - CLK_TCK isn't declared in /sys, just in <time.h>, done here
 */

#define	CLK_TCK	100
#define	CONVTCK(r)	(r.tv_sec * CLK_TCK + r.tv_usec / (1000000 / CLK_TCK))

int
sunos_sys_otimes(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_otimes_args /* {
		syscallarg(struct tms *) tp;
	} */ *uap = v;
	struct tms tms;
	struct rusage ru, *rup;

	/* RUSAGE_SELF */
	calcru(p, &ru.ru_utime, &ru.ru_stime, NULL);
	tms.tms_utime = CONVTCK(ru.ru_utime);
	tms.tms_stime = CONVTCK(ru.ru_stime);

	/* RUSAGE_CHILDREN */
	rup = &p->p_stats->p_cru;
	tms.tms_cutime = CONVTCK(rup->ru_utime);
	tms.tms_cstime = CONVTCK(rup->ru_stime);

	return copyout(&tms, SCARG(uap, tp), sizeof(*(SCARG(uap, tp))));
}
