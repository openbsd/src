/*	$OpenBSD: hpux_compat.c,v 1.21 2002/10/30 20:10:48 millert Exp $	*/
/*	$NetBSD: hpux_compat.c,v 1.35 1997/05/08 16:19:48 mycroft Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: hpux_compat.c 1.64 93/08/05$
 *
 *	@(#)hpux_compat.c	8.4 (Berkeley) 2/13/94
 */

/*
 * Various HP-UX compatibility routines
 */

#ifndef COMPAT_43
#define COMPAT_43
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/ipc.h>
#include <sys/user.h>
#include <sys/mman.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/vmparam.h>

#include <sys/syscallargs.h>

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_sig.h>
#include <compat/hpux/hpux_util.h>
#include <compat/hpux/hpux_termio.h>
#include <compat/hpux/hpux_syscall.h>
#include <compat/hpux/hpux_syscallargs.h>

#include <machine/hpux_machdep.h>

#ifdef DEBUG
int unimpresponse = 0;
#endif

#define NERR	83
#define BERR	1000

/* indexed by BSD errno */
int bsdtohpuxerrnomap[NERR] = {
/*00*/	  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
/*10*/	 10,  45,  12,  13,  14,  15,  16,  17,  18,  19,
/*20*/	 20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
/*30*/	 30,  31,  32,  33,  34, 246, 245, 244, 216, 217,
/*40*/	218, 219, 220, 221, 222, 223, 224, 225, 226, 227,
/*50*/	228, 229, 230, 231, 232, 233, 234, 235, 236, 237,
/*60*/	238, 239, 249, 248, 241, 242, 247,BERR,BERR,BERR,
/*70*/   70,  71,BERR,BERR,BERR,BERR,BERR,  46, 251,BERR,
/*80*/ BERR,BERR,  11
};

extern char sigcode[], esigcode[];
extern struct sysent hpux_sysent[];
extern char *hpux_syscallnames[];

int	hpux_shmctl1(struct proc *, struct hpux_sys_shmctl_args *,
	    register_t *, int);
int	hpuxtobsdioctl(u_long);

static int	hpux_scale(struct timeval *);

/*
 * HP-UX fork and vfork need to map the EAGAIN return value appropriately.
 */
int
hpux_sys_fork(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	/* struct hpux_sys_fork_args *uap = v; */
	int error;

	error = sys_fork(p, v, retval);
	if (error == EAGAIN)
		error = OEAGAIN;
	return (error);
}

int
hpux_sys_vfork(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	/* struct hpux_sys_vfork_args *uap = v; */
	int error;

	error = sys_vfork(p, v, retval);
	if (error == EAGAIN)
		error = OEAGAIN;
	return (error);
}

/*
 * HP-UX versions of wait and wait3 actually pass the parameters
 * (status pointer, options, rusage) into the kernel rather than
 * handling it in the C library stub.  We also need to map any
 * termination signal from BSD to HP-UX.
 */
int
hpux_sys_wait3(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_wait3_args *uap = v;

	/* rusage pointer must be zero */
	if (SCARG(uap, rusage))
		return (EINVAL);
#ifdef m68k
	p->p_md.md_regs[PS] = PSL_ALLCC;
	p->p_md.md_regs[R0] = SCARG(uap, options);
	p->p_md.md_regs[R1] = SCARG(uap, rusage);
#endif

	return (hpux_sys_wait(p, uap, retval));
}

int
hpux_sys_wait(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_wait_args *uap = v;
	struct sys_wait4_args w4;
	int error;
	int sig;
	size_t sz = sizeof(*SCARG(&w4, status));
	int status;

	SCARG(&w4, rusage) = NULL;
	SCARG(&w4, options) = 0;

	if (SCARG(uap, status) == NULL) {
		caddr_t sg = stackgap_init(p->p_emul);
		SCARG(&w4, status) = stackgap_alloc(&sg, sz);
	}
	else
		SCARG(&w4, status) = SCARG(uap, status);

	SCARG(&w4, pid) = WAIT_ANY;

	error = sys_wait4(p, &w4, retval);
	/*
	 * HP-UX wait always returns EINTR when interrupted by a signal
	 * (well, unless its emulating a BSD process, but we don't bother...)
	 */
	if (error == ERESTART)
		error = EINTR;
	if (error)
		return error;

	if ((error = copyin(SCARG(&w4, status), &status, sizeof(status))) != 0)
		return error;

	sig = status & 0xFF;
	if (sig == WSTOPPED) {
		sig = (status >> 8) & 0xFF;
		retval[1] = (bsdtohpuxsig(sig) << 8) | WSTOPPED;
	} else if (sig)
		retval[1] = (status & 0xFF00) |
			bsdtohpuxsig(sig & 0x7F) | (sig & 0x80);

	if (SCARG(uap, status) == NULL)
		return error;
	else
		return copyout(&retval[1], 
			       SCARG(uap, status), sizeof(retval[1]));
}

int
hpux_sys_waitpid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_waitpid_args *uap = v;
	int rv, sig, xstat, error;

	SCARG(uap, rusage) = 0;
	error = sys_wait4(p, uap, retval);
	/*
	 * HP-UX wait always returns EINTR when interrupted by a signal
	 * (well, unless its emulating a BSD process, but we don't bother...)
	 */
	if (error == ERESTART)
		error = EINTR;
	if (error)
		return (error);

	if (SCARG(uap, status)) {
		/*
		 * Wait4 already wrote the status out to user space,
		 * pull it back, change the signal portion, and write
		 * it back out.
		 */
		rv = fuword((caddr_t)SCARG(uap, status));
		if (WIFSTOPPED(rv)) {
			sig = WSTOPSIG(rv);
			rv = W_STOPCODE(bsdtohpuxsig(sig));
		} else if (WIFSIGNALED(rv)) {
			sig = WTERMSIG(rv);
			xstat = WEXITSTATUS(rv);
			rv = W_EXITCODE(xstat, bsdtohpuxsig(sig)) |
				WCOREDUMP(rv);
		}
		(void)suword((caddr_t)SCARG(uap, status), rv);
	}
	return (error);
}

/*
 * Read and write calls.  Same as BSD except for non-blocking behavior.
 * There are three types of non-blocking reads/writes in HP-UX checked
 * in the following order:
 *
 *	O_NONBLOCK: return -1 and errno == EAGAIN
 *	O_NDELAY:   return 0
 *	FIOSNBIO:   return -1 and errno == EWOULDBLOCK
 */
int
hpux_sys_read(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_read_args *uap = v;
	int error;

	error = sys_read(p, (struct sys_read_args *) uap, retval);
	if (error == EWOULDBLOCK) {
		char *fp = &p->p_fd->fd_ofileflags[SCARG(uap, fd)];

		if (*fp & HPUX_UF_NONBLOCK_ON) {
			*retval = -1;
			error = OEAGAIN;
		} else if (*fp & HPUX_UF_FNDELAY_ON) {
			*retval = 0;
			error = 0;
		}
	}
	return (error);
}

int
hpux_sys_write(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_write_args *uap = v;
	int error;

	error = sys_write(p, (struct sys_write_args *) uap, retval);
	if (error == EWOULDBLOCK) {
		char *fp = &p->p_fd->fd_ofileflags[SCARG(uap, fd)];

		if (*fp & HPUX_UF_NONBLOCK_ON) {
			*retval = -1;
			error = OEAGAIN;
		} else if (*fp & HPUX_UF_FNDELAY_ON) {
			*retval = 0;
			error = 0;
		}
	}
	return (error);
}

int
hpux_sys_readv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_readv_args *uap = v;
	int error;

	error = sys_readv(p, (struct sys_readv_args *) uap, retval);
	if (error == EWOULDBLOCK) {
		char *fp = &p->p_fd->fd_ofileflags[SCARG(uap, fd)];

		if (*fp & HPUX_UF_NONBLOCK_ON) {
			*retval = -1;
			error = OEAGAIN;
		} else if (*fp & HPUX_UF_FNDELAY_ON) {
			*retval = 0;
			error = 0;
		}
	}
	return (error);
}

int
hpux_sys_writev(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_writev_args *uap = v;
	int error;

	error = sys_writev(p, (struct sys_writev_args *) uap, retval);
	if (error == EWOULDBLOCK) {
		char *fp = &p->p_fd->fd_ofileflags[SCARG(uap, fd)];

		if (*fp & HPUX_UF_NONBLOCK_ON) {
			*retval = -1;
			error = OEAGAIN;
		} else if (*fp & HPUX_UF_FNDELAY_ON) {
			*retval = 0;
			error = 0;
		}
	}
	return (error);
}

int
hpux_sys_utssys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_utssys_args *uap = v;
	int i;
	int error;
	struct hpux_utsname	ut;
	extern char hostname[], machine[];

	switch (SCARG(uap, request)) {
	/* uname */
	case 0:
		bzero(&ut, sizeof(ut));

		strncpy(ut.sysname, ostype, sizeof(ut.sysname));
		ut.sysname[sizeof(ut.sysname) - 1] = '\0';

		/* copy hostname (sans domain) to nodename */
		for (i = 0; i < 8 && hostname[i] != '.'; i++)
			ut.nodename[i] = hostname[i];
		ut.nodename[i] = '\0';

		strncpy(ut.release, osrelease, sizeof(ut.release));
		ut.release[sizeof(ut.release) - 1] = '\0';

		strncpy(ut.version, version, sizeof(ut.version));
		ut.version[sizeof(ut.version) - 1] = '\0';

		strncpy(ut.machine, machine, sizeof(ut.machine));
		ut.machine[sizeof(ut.machine) - 1] = '\0';

		error = copyout((caddr_t)&ut,
		    (caddr_t)SCARG(uap, uts), sizeof(ut));
		break;

	/* gethostname */
	case 5:
		/* SCARG(uap, dev) is length */
		i = SCARG(uap, dev);
		if (i < 0) {
			error = EINVAL;
			break;
		}
		if (i > hostnamelen + 1)
			i = hostnamelen + 1;
		error = copyout((caddr_t)hostname, (caddr_t)SCARG(uap, uts), i);
		break;

	case 1:	/* ?? */
	case 2:	/* ustat */
	case 3:	/* ?? */
	case 4:	/* sethostname */
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

int
hpux_sys_sysconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sysconf_args *uap = v;
	switch (SCARG(uap, name)) {

	/* clock ticks per second */
	case HPUX_SYSCONF_CLKTICK:
		*retval = hz;
		break;

	/* open files */
	case HPUX_SYSCONF_OPENMAX:
		*retval = NOFILE;
		break;

	/* architecture */
	case HPUX_SYSCONF_CPUTYPE:
		*retval = hpux_cpu_sysconf_arch();
		break;
	default:
		/* XXX */
		uprintf("HP-UX sysconf(%d) not implemented\n",
		    SCARG(uap, name));
		return (EINVAL);
	}
	return (0);
}

int
hpux_sys_ulimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_ulimit_args *uap = v;
	struct rlimit *limp;
	int error = 0;

	limp = &p->p_rlimit[RLIMIT_FSIZE];
	switch (SCARG(uap, cmd)) {
	case 2:
		SCARG(uap, newlimit) *= 512;
		if (SCARG(uap, newlimit) > limp->rlim_max &&
		    (error = suser(p->p_ucred, &p->p_acflag)))
			break;
		limp->rlim_cur = limp->rlim_max = SCARG(uap, newlimit);
		/* else fall into... */

	case 1:
		*retval = limp->rlim_max / 512;
		break;

	case 3:
		limp = &p->p_rlimit[RLIMIT_DATA];
		*retval = ctob(p->p_vmspace->vm_tsize) + limp->rlim_max;
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

/*
 * Map "real time" priorities 0 (high) thru 127 (low) into nice
 * values -16 (high) thru -1 (low).
 */
int
hpux_sys_rtprio(cp, v, retval)
	struct proc *cp;
	void *v;
	register_t *retval;
{
	struct hpux_sys_rtprio_args *uap = v;
	struct proc *p;
	int nice, error;

	if (SCARG(uap, prio) < RTPRIO_MIN && SCARG(uap, prio) > RTPRIO_MAX &&
	    SCARG(uap, prio) != RTPRIO_NOCHG &&
	    SCARG(uap, prio) != RTPRIO_RTOFF)
		return (EINVAL);
	if (SCARG(uap, pid) == 0)
		p = cp;
	else if ((p = pfind(SCARG(uap, pid))) == 0)
		return (ESRCH);
	nice = p->p_nice;
	if (nice < NZERO)
		*retval = (nice + 16) << 3;
	else
		*retval = RTPRIO_RTOFF;
	switch (SCARG(uap, prio)) {

	case RTPRIO_NOCHG:
		return (0);

	case RTPRIO_RTOFF:
		if (nice >= NZERO)
			return (0);
		nice = NZERO;
		break;

	default:
		nice = (SCARG(uap, prio) >> 3) - 16;
		break;
	}
	error = donice(cp, p, nice);
	if (error == EACCES)
		error = EPERM;
	return (error);
}

/* hpux_sys_advise() is found in hpux_machdep.c */

#ifdef PTRACE

int
hpux_sys_ptrace(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_ptrace_args *uap = v;
	int error;
#if defined(PT_READ_U) || defined(PT_WRITE_U)
	int isps = 0;
	struct proc *cp;
#endif

	switch (SCARG(uap, req)) {
	/* map signal */
#if defined(PT_STEP) || defined(PT_CONTINUE)
# ifdef PT_STEP
	case PT_STEP:
# endif
# ifdef PT_CONTINUE
	case PT_CONTINUE:
# endif
		if (SCARG(uap, data)) {
			SCARG(uap, data) = hpuxtobsdsig(SCARG(uap, data));
			if (SCARG(uap, data) == 0)
				SCARG(uap, data) = NSIG;
		}
		break;
#endif
	/* map u-area offset */
#if defined(PT_READ_U) || defined(PT_WRITE_U)
# ifdef PT_READ_U
	case PT_READ_U:
# endif
# ifdef PT_WRITE_U
	case PT_WRITE_U:
# endif
		/*
		 * Big, cheezy hack: hpux_to_bsd_uoff is really intended
		 * to be called in the child context (procxmt) but we
		 * do it here in the parent context to avoid hacks in
		 * the MI sys_process.c file.  This works only because
		 * we can access the child's md_regs pointer and it
		 * has the correct value (the child has already trapped
		 * into the kernel).
		 */
		if ((cp = pfind(SCARG(uap, pid))) == 0)
			return (ESRCH);
		SCARG(uap, addr) =
		    (int *)hpux_to_bsd_uoff(SCARG(uap, addr), &isps, cp);

		/*
		 * Since HP-UX PS is only 16-bits in ar0, requests
		 * to write PS actually contain the PS in the high word
		 * and the high half of the PC (the following register)
		 * in the low word.  Move the PS value to where BSD
		 * expects it.
		 */
		if (isps && SCARG(uap, req) == PT_WRITE_U)
			SCARG(uap, data) >>= 16;
		break;
#endif
	}

	error = sys_ptrace(p, uap, retval);
	/*
	 * Align PS as HP-UX expects it (see WRITE_U comment above).
	 * Note that we do not return the high part of PC like HP-UX
	 * would, but the HP-UX debuggers don't require it.
	 */
#ifdef PT_READ_U
	if (isps && error == 0 && SCARG(uap, req) == PT_READ_U)
		*retval <<= 16;
#endif
	return (error);
}

#endif	/* PTRACE */

#ifdef SYSVSHM
#include <sys/shm.h>

int
hpux_sys_shmctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_shmctl_args *uap = v;

	return (hpux_shmctl1(p, (struct hpux_sys_shmctl_args *)uap, retval, 0));
}

int
hpux_sys_nshmctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;	/* struct hpux_nshmctl_args * */
{
	struct hpux_sys_nshmctl_args *uap = v;

	return (hpux_shmctl1(p, (struct hpux_sys_shmctl_args *)uap, retval, 1));
}

/*
 * Handle HP-UX specific commands.
 */
int
hpux_shmctl1(p, uap, retval, isnew)
	struct proc *p;
	struct hpux_sys_shmctl_args *uap;
	register_t *retval;
	int isnew;
{
	struct shmid_ds *shp;
	struct ucred *cred = p->p_ucred;
	struct hpux_shmid_ds sbuf;
	int error;
	extern struct shmid_ds *shm_find_segment_by_shmid(int);

	if ((shp = shm_find_segment_by_shmid(SCARG(uap, shmid))) == NULL)
		return EINVAL;

	switch (SCARG(uap, cmd)) {
	case SHM_LOCK:
	case SHM_UNLOCK:
		/* don't really do anything, but make them think we did */
		if (cred->cr_uid && cred->cr_uid != shp->shm_perm.uid &&
		    cred->cr_uid != shp->shm_perm.cuid)
			return (EPERM);
		return (0);

	case IPC_STAT:
		if (!isnew)
			break;
		error = ipcperm(cred, &shp->shm_perm, IPC_R);
		if (error == 0) {
			sbuf.shm_perm.uid = shp->shm_perm.uid;
			sbuf.shm_perm.gid = shp->shm_perm.gid;
			sbuf.shm_perm.cuid = shp->shm_perm.cuid;
			sbuf.shm_perm.cgid = shp->shm_perm.cgid;
			sbuf.shm_perm.mode = shp->shm_perm.mode;
			sbuf.shm_perm.seq = shp->shm_perm.seq;
			sbuf.shm_perm.key = shp->shm_perm.key;
			sbuf.shm_segsz = shp->shm_segsz;
			sbuf.shm_ptbl = shp->shm_internal;	/* XXX */
			sbuf.shm_lpid = shp->shm_lpid;
			sbuf.shm_cpid = shp->shm_cpid;
			sbuf.shm_nattch = shp->shm_nattch;
			sbuf.shm_cnattch = shp->shm_nattch;	/* XXX */
			sbuf.shm_atime = shp->shm_atime;
			sbuf.shm_dtime = shp->shm_dtime;
			sbuf.shm_ctime = shp->shm_ctime;
			error = copyout((caddr_t)&sbuf, SCARG(uap, buf),
			    sizeof sbuf);
		}
		return (error);

	case IPC_SET:
		if (!isnew)
			break;
		if (cred->cr_uid && cred->cr_uid != shp->shm_perm.uid &&
		    cred->cr_uid != shp->shm_perm.cuid) {
			return (EPERM);
		}
		error = copyin(SCARG(uap, buf), (caddr_t)&sbuf, sizeof sbuf);
		if (error == 0) {
			shp->shm_perm.uid = sbuf.shm_perm.uid;
			shp->shm_perm.gid = sbuf.shm_perm.gid;
			shp->shm_perm.mode = (shp->shm_perm.mode & ~0777)
				| (sbuf.shm_perm.mode & 0777);
			shp->shm_ctime = time.tv_sec;
		}
		return (error);
	}
	return (sys_shmctl(p, uap, retval));
}
#endif

/*
 * HP-UX mmap() emulation (mainly for shared library support).
 */
int
hpux_sys_mmap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_mmap_args *uap = v;
	struct sys_mmap_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */ nargs;

	SCARG(&nargs, addr) = SCARG(uap, addr);
	SCARG(&nargs, len) = SCARG(uap, len);
	SCARG(&nargs, prot) = SCARG(uap, prot);
	SCARG(&nargs, flags) = SCARG(uap, flags) &
		~(HPUXMAP_FIXED|HPUXMAP_REPLACE|HPUXMAP_ANON);
	if (SCARG(uap, flags) & HPUXMAP_FIXED)
		SCARG(&nargs, flags) |= MAP_FIXED;
	if (SCARG(uap, flags) & HPUXMAP_ANON)
		SCARG(&nargs, flags) |= MAP_ANON;
	SCARG(&nargs, fd) = (SCARG(&nargs, flags) & MAP_ANON) ? -1 : SCARG(uap, fd);
	SCARG(&nargs, pos) = SCARG(uap, pos);

	return (sys_mmap(p, &nargs, retval));
}

int
hpuxtobsdioctl(com)
	u_long com;
{
	switch (com) {
	case HPUXTIOCSLTC:
		com = TIOCSLTC; break;
	case HPUXTIOCGLTC:
		com = TIOCGLTC; break;
	case HPUXTIOCSPGRP:
		com = TIOCSPGRP; break;
	case HPUXTIOCGPGRP:
		com = TIOCGPGRP; break;
	case HPUXTIOCLBIS:
		com = TIOCLBIS; break;
	case HPUXTIOCLBIC:
		com = TIOCLBIC; break;
	case HPUXTIOCLSET:
		com = TIOCLSET; break;
	case HPUXTIOCLGET:
		com = TIOCLGET; break;
	case HPUXTIOCGWINSZ:
		com = TIOCGWINSZ; break;
	case HPUXTIOCSWINSZ:
		com = TIOCSWINSZ; break;
	}
	return(com);
}

/*
 * HP-UX ioctl system call.  The differences here are:
 *	IOC_IN also means IOC_VOID if the size portion is zero.
 *	no FIOCLEX/FIONCLEX/FIOASYNC/FIOGETOWN/FIOSETOWN
 *	the sgttyb struct is 2 bytes longer
 */
int
hpux_sys_ioctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(int) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	int com, error = 0;
	u_int size;
	caddr_t memp = 0;
#define STK_PARAMS	128
	char stkbuf[STK_PARAMS];
	caddr_t dt = stkbuf;

	com = SCARG(uap, com);

	/* XXX */
	if (com == HPUXTIOCGETP || com == HPUXTIOCSETP)
		return (getsettty(p, SCARG(uap, fd), com, SCARG(uap, data)));

	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);
	if ((fp->f_flag & (FREAD|FWRITE)) == 0)
		return (EBADF);

	/*
	 * Interpret high order word to find
	 * amount of data to be copied to/from the
	 * user's address space.
	 */
	size = IOCPARM_LEN(com);
	if (size > IOCPARM_MAX)
		return (ENOTTY);
	FREF(fp);
	if (size > sizeof (stkbuf)) {
		memp = (caddr_t)malloc((u_long)size, M_IOCTLOPS, M_WAITOK);
		dt = memp;
	}
	if (com&IOC_IN) {
		if (size) {
			error = copyin(SCARG(uap, data), dt, (u_int)size);
			if (error) {
				goto out;
			}
		} else
			*(caddr_t *)dt = SCARG(uap, data);
	} else if ((com&IOC_OUT) && size)
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		bzero(dt, size);
	else if (com&IOC_VOID)
		*(caddr_t *)dt = SCARG(uap, data);

	switch (com) {

	case HPUXFIOSNBIO:
	{
		char *ofp = &fdp->fd_ofileflags[SCARG(uap, fd)];
		int tmp;

		if (*(int *)dt)
			*ofp |= HPUX_UF_FIONBIO_ON;
		else
			*ofp &= ~HPUX_UF_FIONBIO_ON;
		/*
		 * Only set/clear if O_NONBLOCK/FNDELAY not in effect
		 */
		if ((*ofp & (HPUX_UF_NONBLOCK_ON|HPUX_UF_FNDELAY_ON)) == 0) {
			tmp = *ofp & HPUX_UF_FIONBIO_ON;
			error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO,
						       (caddr_t)&tmp, p);
		}
		break;
	}

	case HPUXTIOCCONS:
		*(int *)dt = 1;
		error = (*fp->f_ops->fo_ioctl)(fp, TIOCCONS, dt, p);
		break;

	/* BSD-style job control ioctls */
	case HPUXTIOCLBIS:
	case HPUXTIOCLBIC:
	case HPUXTIOCLSET:
		*(int *)dt &= HPUXLTOSTOP;
		if (*(int *)dt & HPUXLTOSTOP)
			*(int *)dt = LTOSTOP;
		/* fall into */

	/* simple mapping cases */
	case HPUXTIOCLGET:
	case HPUXTIOCSLTC:
	case HPUXTIOCGLTC:
	case HPUXTIOCSPGRP:
	case HPUXTIOCGPGRP:
	case HPUXTIOCGWINSZ:
	case HPUXTIOCSWINSZ:
		error = (*fp->f_ops->fo_ioctl)
			(fp, hpuxtobsdioctl(com), dt, p);
		if (error == 0 && com == HPUXTIOCLGET) {
			*(int *)dt &= LTOSTOP;
			if (*(int *)dt & LTOSTOP)
				*(int *)dt = HPUXLTOSTOP;
		}
		break;

	/* SYS 5 termio and POSIX termios */
	case HPUXTCGETA:
	case HPUXTCSETA:
	case HPUXTCSETAW:
	case HPUXTCSETAF:
	case HPUXTCGETATTR:
	case HPUXTCSETATTR:
	case HPUXTCSETATTRD:
	case HPUXTCSETATTRF:
		error = hpux_termio(SCARG(uap, fd), com, dt, p);
		break;

	default:
		error = (*fp->f_ops->fo_ioctl)(fp, com, dt, p);
		break;
	}
	/*
	 * Copy any data to user, size was
	 * already set and checked above.
	 */
	if (error == 0 && (com&IOC_OUT) && size)
		error = copyout(dt, SCARG(uap, data), (u_int)size);

out:
	FRELE(fp);
	if (memp)
		free(memp, M_IOCTLOPS);
	return (error);
}

/* hpux_sys_getcontext() is found in hpux_machdep.c */

/*
 * This is the equivalent of BSD getpgrp but with more restrictions.
 * Note we do not check the real uid or "saved" uid.
 */
int
hpux_sys_getpgrp2(cp, v, retval)
	struct proc *cp;
	void *v;
	register_t *retval;
{
	struct hpux_sys_getpgrp2_args *uap = v;
	struct proc *p;

	if (SCARG(uap, pid) == 0)
		SCARG(uap, pid) = cp->p_pid;
	p = pfind(SCARG(uap, pid));
	if (p == 0)
		return (ESRCH);
	if (cp->p_ucred->cr_uid && p->p_ucred->cr_uid != cp->p_ucred->cr_uid &&
	    !inferior(p))
		return (EPERM);
	*retval = p->p_pgid;
	return (0);
}

/*
 * This is the equivalent of BSD setpgrp but with more restrictions.
 * Note we do not check the real uid or "saved" uid or pgrp.
 */
int
hpux_sys_setpgrp2(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_setpgrp2_args *uap = v;

	/* empirically determined */
	if (SCARG(uap, pgid) < 0 || SCARG(uap, pgid) >= 30000)
		return (EINVAL);
	return (sys_setpgid(p, uap, retval));
}

int
hpux_sys_getrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_getrlimit_args *uap = v;
	struct compat_43_sys_getrlimit_args ap;

	if (SCARG(uap, which) > HPUXRLIMIT_NOFILE)
		return (EINVAL);
	if (SCARG(uap, which) == HPUXRLIMIT_NOFILE)
		SCARG(uap, which) = RLIMIT_NOFILE;

	SCARG(&ap, which) = SCARG(uap, which);
	SCARG(&ap, rlp) = SCARG(uap, rlp);

	return (compat_43_sys_getrlimit(p, uap, retval));
}

int
hpux_sys_setrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_setrlimit_args *uap = v;
	struct compat_43_sys_setrlimit_args ap;

	if (SCARG(uap, which) > HPUXRLIMIT_NOFILE)
		return (EINVAL);
	if (SCARG(uap, which) == HPUXRLIMIT_NOFILE)
		SCARG(uap, which) = RLIMIT_NOFILE;

	SCARG(&ap, which) = SCARG(uap, which);
	SCARG(&ap, rlp) = SCARG(uap, rlp);

	return (compat_43_sys_setrlimit(p, uap, retval));
}

/*
 * XXX: simple recognition hack to see if we can make grmd work.
 */
int
hpux_sys_lockf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	/* struct hpux_sys_lockf_args *uap = v; */

	return (0);
}

int
hpux_sys_getaccess(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_getaccess_args *uap = v;
	int lgroups[NGROUPS];
	int error = 0;
	struct ucred *cred;
	struct vnode *vp;
	struct nameidata nd;

	/*
	 * Build an appropriate credential structure
	 */
	cred = crdup(p->p_ucred);
	switch (SCARG(uap, uid)) {
	case 65502:	/* UID_EUID */
		break;
	case 65503:	/* UID_RUID */
		cred->cr_uid = p->p_cred->p_ruid;
		break;
	case 65504:	/* UID_SUID */
		error = EINVAL;
		break;
	default:
		if (SCARG(uap, uid) > 65504)
			error = EINVAL;
		cred->cr_uid = SCARG(uap, uid);
		break;
	}
	switch (SCARG(uap, ngroups)) {
	case -1:	/* NGROUPS_EGID */
		cred->cr_ngroups = 1;
		break;
	case -5:	/* NGROUPS_EGID_SUPP */
		break;
	case -2:	/* NGROUPS_RGID */
		cred->cr_ngroups = 1;
		cred->cr_gid = p->p_cred->p_rgid;
		break;
	case -6:	/* NGROUPS_RGID_SUPP */
		cred->cr_gid = p->p_cred->p_rgid;
		break;
	case -3:	/* NGROUPS_SGID */
	case -7:	/* NGROUPS_SGID_SUPP */
		error = EINVAL;
		break;
	case -4:	/* NGROUPS_SUPP */
		if (cred->cr_ngroups > 1)
			cred->cr_gid = cred->cr_groups[1];
		else
			error = EINVAL;
		break;
	default:
		if (SCARG(uap, ngroups) > 0 && SCARG(uap, ngroups) <= NGROUPS)
			error = copyin((caddr_t)SCARG(uap, gidset),
				       (caddr_t)&lgroups[0],
				       SCARG(uap, ngroups) *
					   sizeof(lgroups[0]));
		else
			error = EINVAL;
		if (error == 0) {
			int gid;

			for (gid = 0; gid < SCARG(uap, ngroups); gid++)
				cred->cr_groups[gid] = lgroups[gid];
			cred->cr_ngroups = SCARG(uap, ngroups);
		}
		break;
	}
	/*
	 * Lookup file using caller's effective IDs.
	 */
	if (error == 0) {
		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
			SCARG(uap, path), p);
		error = namei(&nd);
	}
	if (error) {
		crfree(cred);
		return (error);
	}
	/*
	 * Use the constructed credentials for access checks.
	 */
	vp = nd.ni_vp;
	*retval = 0;
	if (VOP_ACCESS(vp, VREAD, cred, p) == 0)
		*retval |= R_OK;
	if (vn_writechk(vp) == 0 && VOP_ACCESS(vp, VWRITE, cred, p) == 0)
		*retval |= W_OK;
	if (VOP_ACCESS(vp, VEXEC, cred, p) == 0)
		*retval |= X_OK;
	vput(vp);
	crfree(cred);
	return (error);
}

/* hpux_to_bsd_uoff() is found in hpux_machdep.c */

/*
 * Ancient HP-UX system calls.  Some 9.x executables even use them!
 */
#define HPUX_HZ	50

#include <sys/times.h>


/*
 * SYS V style setpgrp()
 */
int
hpux_sys_setpgrp_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	if (p->p_pid != p->p_pgid)
		enterpgrp(p, p->p_pid, 0);
	*retval = p->p_pgid;
	return (0);
}

int
hpux_sys_time_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_time_6x_args /* {
		syscallarg(time_t *) t;
	} */ *uap = v;
	int error = 0;
	struct timeval tv;

	microtime(&tv);
        if (SCARG(uap, t) != NULL)
		error = copyout(&tv.tv_sec, SCARG(uap, t), sizeof(time_t));

	*retval = (register_t)tv.tv_sec;
	return (error);
}

int
hpux_sys_stime_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_stime_6x_args /* {
		syscallarg(int) time;
	} */ *uap = v;
	struct timeval tv;
	int s, error;

	tv.tv_sec = SCARG(uap, time);
	tv.tv_usec = 0;
	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);

	/* WHAT DO WE DO ABOUT PENDING REAL-TIME TIMEOUTS??? */
	boottime.tv_sec += tv.tv_sec - time.tv_sec;
	s = splhigh(); time = tv; splx(s);
	resettodr();
	return (0);
}

int
hpux_sys_ftime_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_ftime_6x_args /* {
		syscallarg(struct hpux_timeb *) tp;
	} */ *uap = v;
	struct hpux_otimeb tb;
	int s;

	s = splhigh();
	tb.time = time.tv_sec;
	tb.millitm = time.tv_usec / 1000;
	splx(s);
	tb.timezone = tz.tz_minuteswest;
	tb.dstflag = tz.tz_dsttime;
	return (copyout((caddr_t)&tb, (caddr_t)SCARG(uap, tp), sizeof (tb)));
}

int
hpux_sys_alarm_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_alarm_6x_args /* {
		syscallarg(int) deltat;
	} */ *uap = v;
	int s = splhigh();
	int timo;

	timeout_del(&p->p_realit_to);
	timerclear(&p->p_realtimer.it_interval);
	*retval = 0;
	if (timerisset(&p->p_realtimer.it_value) &&
	    timercmp(&p->p_realtimer.it_value, &time, >))
		*retval = p->p_realtimer.it_value.tv_sec - time.tv_sec;
	if (SCARG(uap, deltat) == 0) {
		timerclear(&p->p_realtimer.it_value);
		splx(s);
		return (0);
	}
	p->p_realtimer.it_value = time;
	p->p_realtimer.it_value.tv_sec += SCARG(uap, deltat);
	timo = hzto(&p->p_realtimer.it_value);
	if (timo <= 0)
		timo = 1;
	timeout_add(&p->p_realit_to, timo);
	splx(s);
	return (0);
}

int
hpux_sys_nice_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_nice_6x_args /* {
		syscallarg(int) nval;
	} */ *uap = v;
	int error;

	error = donice(p, p, (p->p_nice-NZERO)+SCARG(uap, nval));
	if (error == 0)
		*retval = p->p_nice - NZERO;
	return (error);
}

int
hpux_sys_times_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_times_6x_args /* {
		syscallarg(struct tms *) tms;
	} */ *uap = v;
	struct timeval ru, rs;
	struct tms atms;
	int error;

	calcru(p, &ru, &rs, NULL);
	atms.tms_utime = hpux_scale(&ru);
	atms.tms_stime = hpux_scale(&rs);
	atms.tms_cutime = hpux_scale(&p->p_stats->p_cru.ru_utime);
	atms.tms_cstime = hpux_scale(&p->p_stats->p_cru.ru_stime);
	error = copyout((caddr_t)&atms, (caddr_t)SCARG(uap, tms),
	    sizeof (atms));
	if (error == 0)
		*(time_t *)retval = hpux_scale((struct timeval *)&time) -
		    hpux_scale(&boottime);
	return (error);
}

/*
 * Doesn't exactly do what the documentation says.
 * What we really do is return 1/HPUX_HZ-th of a second since that
 * is what HP-UX returns.
 */
static int
hpux_scale(tvp)
	struct timeval *tvp;
{
	return (tvp->tv_sec * HPUX_HZ + tvp->tv_usec * HPUX_HZ / 1000000);
}

/*
 * Set IUPD and IACC times on file.
 * Can't set ICHG.
 */
int
hpux_sys_utime_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_utime_6x_args /* {
		syscallarg(char *) fname;
		syscallarg(time_t *) tptr;
	} */ *uap = v;
	struct vnode *vp;
	struct vattr vattr;
	time_t tv[2];
	int error;
	struct nameidata nd;

	if (SCARG(uap, tptr)) {
		error = copyin((caddr_t)SCARG(uap, tptr), (caddr_t)tv,
		    sizeof (tv));
		if (error)
			return (error);
	} else
		tv[0] = tv[1] = time.tv_sec;
	vattr_null(&vattr);
	vattr.va_atime.tv_sec = tv[0];
	vattr.va_atime.tv_nsec = 0;
	vattr.va_mtime.tv_sec = tv[1];
	vattr.va_mtime.tv_nsec = 0;
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, fname), p);
	if ((error = namei(&nd)))
		return (error);
	vp = nd.ni_vp;
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else
		error = VOP_SETATTR(vp, &vattr, nd.ni_cnd.cn_cred, p);
	vput(vp);
	return (error);
}

int
hpux_sys_pause_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigsuspend_args bsa;

	SCARG(&bsa, mask) = p->p_sigmask;
	return (sys_sigsuspend(p, &bsa, retval));
}
