/*	$OpenBSD: ultrix_misc.c,v 1.30 2007/06/06 17:15:13 deraadt Exp $	*/
/*	$NetBSD: ultrix_misc.c,v 1.23 1996/04/07 17:23:04 jonathan Exp $	*/

/*
 * Copyright (c) 1995
 *	Jonathan Stone (hereinafter referred to as the author)
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
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
 * 3. Neither the name of the University nor the names of its contributors
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
#include <sys/dirent.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
/*#include <sys/stat.h>*/
/*#include <sys/ioctl.h>*/
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
#include <compat/ultrix/ultrix_util.h>

#include <netinet/in.h>

#include <miscfs/specfs/specdev.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <uvm/uvm_extern.h>

#include <sys/conf.h>					/* iszerodev() */
#include <sys/socketvar.h>				/* sosetopt() */

extern struct sysent ultrix_sysent[];
#ifdef SYSCALL_DEBUG
extern char *ultrix_syscallnames[];
#endif

/*
 * Select the appropriate setregs callback for the target architecture.
 */
#ifdef __mips__
#define ULTRIX_EXEC_SETREGS cpu_exec_ecoff_setregs
#endif /* __mips__ */

#ifdef __vax__
#define ULTRIX_EXEC_SETREGS setregs
#endif /* __vax__ */


extern void ULTRIX_EXEC_SETREGS(struct proc *, struct exec_package *,
					u_long, register_t *);
extern char sigcode[], esigcode[];

struct emul emul_ultrix = {
	"ultrix",
	NULL,
	sendsig,
	ULTRIX_SYS_syscall,
	ULTRIX_SYS_MAXSYSCALL,
	ultrix_sysent,
#ifdef SYSCALL_DEBUG
	ultrix_syscallnames,
#else
	NULL,
#endif
	0,
	copyargs,
	ULTRIX_EXEC_SETREGS,
	NULL,
	sigcode,
	esigcode,
};

#define GSI_PROG_ENV 1

int
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

int
ultrix_sys_setsysinfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

#ifdef notyet
	struct ultrix_sys_setsysinfo_args *uap = v;
#endif

	*retval = 0;
	return 0;
}

int
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

int
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

/*
 * Ultrix binaries pass in FD_MAX as the first arg to select().
 * On Ultrix, FD_MAX is 4096, which is more than the NetBSD sys_select()
 * can handle.
 * Since we can't have more than the (native) FD_MAX descriptors open, 
 * limit nfds to at most FD_MAX.
 */
int
ultrix_sys_select(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_select_args *uap = v;
	struct timeval atv;
	int error;

	/* Limit number of FDs selected on to the native maximum */

	if (SCARG(uap, nd) > FD_SETSIZE)
		SCARG(uap, nd) = FD_SETSIZE;

	/* Check for negative timeval */
	if (SCARG(uap, tv)) {
		error = copyin((caddr_t)SCARG(uap, tv), (caddr_t)&atv,
			       sizeof(atv));
		if (error)
			goto done;
#ifdef DEBUG
		/* Ultrix clients sometimes give negative timeouts? */
		if (atv.tv_sec < 0 || atv.tv_usec < 0)
			printf("ultrix select( %ld, %ld): negative timeout\n",
			       atv.tv_sec, atv.tv_usec);
		/*tvp = (timeval *)STACKGAPBASE;*/
#endif

	}
	error = sys_select(p, (void *) uap, retval);
	if (error == EINVAL)
		printf("ultrix select: bad args?\n");

done:
	return error;
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


#define	SUN__MAP_NEW	0x80000000	/* if not, old mmap & cannot handle */

int
ultrix_sys_mmap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_mmap_args *uap = v;
	struct sys_mmap_args ouap;

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
	    SCARG(&ouap, addr) < (void *)round_page((vaddr_t)p->p_vmspace->vm_daddr+MAXDSIZ))
		SCARG(&ouap, addr) = (void *)round_page((vaddr_t)p->p_vmspace->vm_daddr+MAXDSIZ);

	SCARG(&ouap, len) = SCARG(uap, len);
	SCARG(&ouap, prot) = SCARG(uap, prot);
	SCARG(&ouap, fd) = SCARG(uap, fd);
	SCARG(&ouap, pos) = SCARG(uap, pos);

	return (sys_mmap(p, &ouap, retval));
}

int
ultrix_sys_setsockopt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct ultrix_sys_setsockopt_args *uap = v;
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp))  != 0)
		return (error);
#define	SO_DONTLINGER (~SO_LINGER)
	if (SCARG(uap, name) == SO_DONTLINGER) {
		m = m_get(M_WAIT, MT_SOOPTS);
		mtod(m, struct linger *)->l_onoff = 0;
		m->m_len = sizeof(struct linger);
		error = (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
		    SO_LINGER, m));
		goto bad;
	}
	if (SCARG(uap, valsize) > MLEN) {
		error = EINVAL;
		goto bad;
	}
	if (SCARG(uap, val)) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if ((error = copyin(SCARG(uap, val), mtod(m, caddr_t),
				    (u_int)SCARG(uap, valsize))) != 0) {
			(void) m_free(m);
			goto bad;
		}
		m->m_len = SCARG(uap, valsize);
	}
	error = (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
	    SCARG(uap, name), m));
bad:
	FRELE(fp);
	return (error);
}

struct ultrix_utsname {
	char    sysname[9];
	char    nodename[9];
	char    nodeext[65-9];
	char    release[9];
	char    version[9];
	char    machine[9];
};

int
ultrix_sys_uname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_uname_args *uap = v;
	struct ultrix_utsname sut;
	extern char machine[];

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

#if defined (NFSSERVER)
int
ultrix_sys_nfssvc(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

#if 0	/* XXX */
	struct ultrix_sys_nfssvc_args *uap = v;
	struct emul *e = p->p_emul;
	struct sys_nfssvc_args outuap;
	struct sockaddr sa;
	int error;

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
	int32_t	f_tfree;	/* total free */
	ino_t	f_tinode;	/* total inodes free */
	char	f_fname[6];	/* filsys name */
	char	f_fpack[6];	/* filsys pack name */
};

int
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

	if ((error = copyout(&us, SCARG(uap, buf), sizeof us)) != 0)
		return (error);
	return 0;
}

int
ultrix_sys_quotactl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

#ifdef notyet
	struct ultrix_sys_quotactl_args *uap = v;
#endif

	return EINVAL;
}

int
ultrix_sys_vhangup(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return 0;
}

int
ultrix_sys_exportfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#ifdef notyet
	struct ultrix_sys_exportfs_args *uap = v;
#endif

	/*
	 * XXX: should perhaps translate into a mount(2)
	 * with MOUNT_EXPORT?
	 */
	return 0;
}

int
ultrix_sys_sigpending(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_sigpending_args *uap = v;
	int mask = p->p_siglist & p->p_sigmask;

	return (copyout((caddr_t)&mask, (caddr_t)SCARG(uap, mask), sizeof(int)));
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

int
ultrix_sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_sigcleanup_args *uap = v;

#ifdef DEBUG
	printf("ultrix sigreturn\n");
#endif
	return sys_sigreturn(p, (struct sys_sigreturn_args  *)uap, retval);
}

int
ultrix_sys_execve(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_execve_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argv;
		syscallarg(char **) envp;
        } */ *uap = v;
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	ULTRIX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = SCARG(uap, envp);

	return (sys_execve(p, &ap, retval));
}
