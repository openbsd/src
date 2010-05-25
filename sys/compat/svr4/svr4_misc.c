/*	$OpenBSD: svr4_misc.c,v 1.53 2010/05/25 15:25:17 millert Exp $	 */
/*	$NetBSD: svr4_misc.c,v 1.42 1996/12/06 03:22:34 christos Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
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

/*
 * SVR4 compatibility module.
 *
 * SVR4 system calls that are implemented differently in BSD are
 * handled here.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/exec_olf.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/ktrace.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/pool.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/unistd.h>
#include <sys/times.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>

#include <netinet/in.h>
#include <sys/syscallargs.h>

#include <miscfs/specfs/specdev.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_time.h>
#include <compat/svr4/svr4_dirent.h>
#include <compat/svr4/svr4_ulimit.h>
#include <compat/svr4/svr4_hrt.h>
#include <compat/svr4/svr4_wait.h>
#include <compat/svr4/svr4_statvfs.h>
#include <compat/svr4/svr4_sysconfig.h>
#include <compat/svr4/svr4_acl.h>

#include <compat/common/compat_dir.h>

#include <uvm/uvm_extern.h>

static __inline clock_t timeval_to_clock_t(struct timeval *);
static int svr4_setinfo(struct proc *, int, svr4_siginfo_t *);

struct svr4_hrtcntl_args;
static int svr4_hrtcntl(struct proc *, struct svr4_hrtcntl_args *,
    register_t *);
static void bsd_statfs_to_svr4_statvfs(const struct statfs *,
    struct svr4_statvfs *);
static void bsd_statfs_to_svr4_statvfs64(const struct statfs *,
    struct svr4_statvfs64 *);
static struct proc *svr4_pfind(pid_t pid);

static int svr4_mknod(struct proc *, register_t *, char *,
			   svr4_mode_t, svr4_dev_t);

int
svr4_sys_wait(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_wait_args *uap = v;
	struct sys_wait4_args w4;
	int error;
	size_t sz = sizeof(*SCARG(&w4, status));

	SCARG(&w4, rusage) = NULL;
	SCARG(&w4, options) = 0;

	if (SCARG(uap, status) == NULL) {
		caddr_t sg = stackgap_init(p->p_emul);
		SCARG(&w4, status) = stackgap_alloc(&sg, sz);
	}
	else
		SCARG(&w4, status) = SCARG(uap, status);

	SCARG(&w4, pid) = WAIT_ANY;

	if ((error = sys_wait4(p, &w4, retval)) != 0)
		return error;

	/*
	 * It looks like wait(2) on svr4/solaris/2.4 returns
	 * the status in retval[1], and the pid on retval[0].
	 * NB: this can break if register_t stops being an int.
	 */
	return copyin(SCARG(&w4, status), &retval[1], sz);
}


int
svr4_sys_execv(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_execv_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argv;
	} */ *uap = v;
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = NULL;

	return sys_execve(p, &ap, retval);
}


int
svr4_sys_execve(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_execve_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argv;
		syscallarg(char **) envp;
        } */ *uap = v;
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = SCARG(uap, envp);

	return sys_execve(p, &ap, retval);
}


int
svr4_sys_time(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_time_args *uap = v;
	int error = 0;
	struct timeval tv;

	microtime(&tv);
	if (SCARG(uap, t))
		error = copyout(&tv.tv_sec, SCARG(uap, t),
				sizeof(*(SCARG(uap, t))));
	*retval = (int) tv.tv_sec;

	return error;
}


/*
 * Read SVR4-style directory entries.  We suck them into kernel space so
 * that they can be massaged before being copied out to user code.  Like
 * SunOS, we squish out `empty' entries.
 *
 * This is quite ugly, but what do you expect from compatibility code?
 */

int svr4_readdir_callback(void *, struct dirent *, off_t);
int svr4_readdir64_callback(void *, struct dirent *, off_t);

struct svr4_readdir_callback_args {
	caddr_t outp;
	int     resid;
};

int
svr4_readdir_callback(arg, bdp, cookie)
	void *arg;
	struct dirent *bdp;
	off_t cookie;
{
	struct svr4_dirent idb;
	struct svr4_readdir_callback_args *cb = arg; 
	int svr4_reclen;
	int error;

	svr4_reclen = SVR4_RECLEN(&idb, bdp->d_namlen);
	if (cb->resid < svr4_reclen)
		return (ENOMEM);

	idb.d_ino = (svr4_ino_t)bdp->d_fileno;
	idb.d_off = (svr4_off_t)cookie;
	idb.d_reclen = (u_short)svr4_reclen;
	strlcpy(idb.d_name, bdp->d_name, sizeof(idb.d_name));
	if ((error = copyout((caddr_t)&idb, cb->outp, svr4_reclen)))
		return (error);

	cb->outp += svr4_reclen;
	cb->resid -= svr4_reclen;

	return (0);
}

int
svr4_readdir64_callback(arg, bdp, cookie)
	void *arg;
	struct dirent *bdp;
	off_t cookie;
{
	struct svr4_dirent64 idb;
	struct svr4_readdir_callback_args *cb = arg; 
	int svr4_reclen;
	int error;

	svr4_reclen = SVR4_RECLEN(&idb, bdp->d_namlen);
	if (cb->resid < svr4_reclen)
		return (ENOMEM);

	/*
	 * Massage in place to make a SVR4-shaped dirent (otherwise
	 * we have to worry about touching user memory outside of
	 * the copyout() call).
	 */
	idb.d_ino = (svr4_ino64_t)bdp->d_fileno;
	idb.d_off = (svr4_off64_t)cookie;
	idb.d_reclen = (u_short)svr4_reclen;
	strlcpy(idb.d_name, bdp->d_name, sizeof(idb.d_name));
	if ((error = copyout((caddr_t)&idb, cb->outp, svr4_reclen)))
		return (error);

	cb->outp += svr4_reclen;
	cb->resid -= svr4_reclen;

	return (0);
}


int
svr4_sys_getdents(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_getdents_args *uap = v;
	struct svr4_readdir_callback_args args;
	struct file *fp;
	int error;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);

	args.resid = SCARG(uap, nbytes);
	args.outp = (caddr_t)SCARG(uap, buf);

	error = readdir_with_callback(fp, &fp->f_offset, SCARG(uap, nbytes),
	    svr4_readdir_callback, &args);
	FRELE(fp);
	if (error)
		return (error);

	*retval = SCARG(uap, nbytes) - args.resid;

	return (0);
}

int
svr4_sys_getdents64(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_getdents64_args *uap = v;
	struct svr4_readdir_callback_args args;
	struct file *fp;
	int error;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);

	args.resid = SCARG(uap, nbytes);
	args.outp = (caddr_t)SCARG(uap, dp);

	error = readdir_with_callback(fp, &fp->f_offset, SCARG(uap, nbytes),
	    svr4_readdir64_callback, &args);
	FRELE(fp);
	if (error)
		return (error);

	*retval = SCARG(uap, nbytes) - args.resid;

	return (0);
}

int
svr4_sys_mmap(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_mmap_args	*uap = v;
	struct sys_mmap_args	 mm;
	void			*rp;
#define _MAP_NEW	0x80000000
	/*
         * Verify the arguments.
         */
	if (SCARG(uap, prot) & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return EINVAL;	/* XXX still needed? */

	if (SCARG(uap, len) == 0)
		return EINVAL;

	SCARG(&mm, prot) = SCARG(uap, prot);
	SCARG(&mm, len) = SCARG(uap, len);
	SCARG(&mm, flags) = SCARG(uap, flags) & ~_MAP_NEW;
	SCARG(&mm, fd) = SCARG(uap, fd);
	SCARG(&mm, addr) = SCARG(uap, addr);
	SCARG(&mm, pos) = SCARG(uap, pos);

	rp = (void *) round_page((vaddr_t)p->p_vmspace->vm_daddr + MAXDSIZ);
	if ((SCARG(&mm, flags) & MAP_FIXED) == 0 &&
	    SCARG(&mm, addr) != 0 && SCARG(&mm, addr) < rp)
		SCARG(&mm, addr) = rp;

	return sys_mmap(p, &mm, retval);
}

int
svr4_sys_mmap64(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_mmap64_args	*uap = v;
	struct sys_mmap_args	 mm;
	void			*rp;
#define _MAP_NEW	0x80000000
	/*
         * Verify the arguments.
         */
	if (SCARG(uap, prot) & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return EINVAL;	/* XXX still needed? */

	if (SCARG(uap, len) == 0)
		return EINVAL;

	SCARG(&mm, prot) = SCARG(uap, prot);
	SCARG(&mm, len) = SCARG(uap, len);
	SCARG(&mm, flags) = SCARG(uap, flags) & ~_MAP_NEW;
	SCARG(&mm, fd) = SCARG(uap, fd);
	SCARG(&mm, addr) = SCARG(uap, addr);
	SCARG(&mm, pos) = SCARG(uap, pos);

	rp = (void *) round_page((vaddr_t)p->p_vmspace->vm_daddr + MAXDSIZ);
	if ((SCARG(&mm, flags) & MAP_FIXED) == 0 &&
	    SCARG(&mm, addr) != 0 && SCARG(&mm, addr) < rp)
		SCARG(&mm, addr) = rp;

	return sys_mmap(p, &mm, retval);
}

int
svr4_sys_fchroot(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_fchroot_args *uap = v;
	struct filedesc	*fdp = p->p_fd;
	struct vnode	*vp;
	struct file	*fp;
	int		 error;

	if ((error = suser(p, 0)) != 0)
		return error;
	if ((error = getvnode(fdp, SCARG(uap, fd), &fp)) != 0)
		return error;

	vp = (struct vnode *) fp->f_data;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	VOP_UNLOCK(vp, 0, p);
	if (error) {
		FRELE(fp);
		return error;
	}
	vref(vp);
	if (fdp->fd_rdir != NULL)
		vrele(fdp->fd_rdir);
	fdp->fd_rdir = vp;
	FRELE(fp);
	return 0;
}

static int
svr4_mknod(p, retval, path, mode, dev)
	struct proc *p;
	register_t *retval;
	char *path;
	svr4_mode_t mode;
	svr4_dev_t dev;
{
	caddr_t sg = stackgap_init(p->p_emul);

	SVR4_CHECK_ALT_EXIST(p, &sg, path);

	if (S_ISFIFO(mode)) {
		struct sys_mkfifo_args ap;
		SCARG(&ap, path) = path;
		SCARG(&ap, mode) = mode;
		return sys_mkfifo(p, &ap, retval);
	} else {
		struct sys_mknod_args ap;
		SCARG(&ap, path) = path;
		SCARG(&ap, mode) = mode;
		SCARG(&ap, dev) = dev;
		return sys_mknod(p, &ap, retval);
	}
}


int
svr4_sys_mknod(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_mknod_args *uap = v;
	return svr4_mknod(p, retval,
			  SCARG(uap, path), SCARG(uap, mode),
			  svr4_to_bsd_odev_t(SCARG(uap, dev)));
}


int
svr4_sys_xmknod(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_xmknod_args *uap = v;
	return svr4_mknod(p, retval,
			  SCARG(uap, path), SCARG(uap, mode),
			  svr4_to_bsd_dev_t(SCARG(uap, dev)));
}


int
svr4_sys_vhangup(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return 0;
}


int
svr4_sys_sysconfig(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_sysconfig_args *uap = v;
	extern int	maxfiles;

	switch (SCARG(uap, name)) {
	case SVR4_CONFIG_UNUSED:
		*retval = 0;
		break;
	case SVR4_CONFIG_NGROUPS:
		*retval = NGROUPS_MAX;
		break;
	case SVR4_CONFIG_CHILD_MAX:
		*retval = maxproc;
		break;
	case SVR4_CONFIG_OPEN_FILES:
		*retval = maxfiles;
		break;
	case SVR4_CONFIG_POSIX_VER:
		*retval = 198808;
		break;
	case SVR4_CONFIG_PAGESIZE:
		*retval = NBPG;
		break;
	case SVR4_CONFIG_CLK_TCK:
		*retval = 60;	/* should this be `hz', ie. 100? */
		break;
	case SVR4_CONFIG_XOPEN_VER:
		*retval = 2;	/* XXX: What should that be? */
		break;
	case SVR4_CONFIG_PROF_TCK:
		*retval = 60;	/* XXX: What should that be? */
		break;
	case SVR4_CONFIG_NPROC_CONF:
		*retval = 1;	/* Only one processor for now */
		break;
	case SVR4_CONFIG_NPROC_ONLN:
		*retval = 1;	/* And it better be online */
		break;
	case SVR4_CONFIG_AIO_LISTIO_MAX:
	case SVR4_CONFIG_AIO_MAX:
	case SVR4_CONFIG_AIO_PRIO_DELTA_MAX:
		*retval = 0;	/* No aio support */
		break;
	case SVR4_CONFIG_DELAYTIMER_MAX:
		*retval = 0;	/* No delaytimer support */
		break;
#ifdef SYSVMSG
	case SVR4_CONFIG_MQ_OPEN_MAX:
		*retval = msginfo.msgmni;
		break;
#endif
	case SVR4_CONFIG_MQ_PRIO_MAX:
		*retval = 0;	/* XXX: Don't know */
		break;
	case SVR4_CONFIG_RTSIG_MAX:
		*retval = 0;
		break;
#ifdef SYSVSEM
	case SVR4_CONFIG_SEM_NSEMS_MAX:
		*retval = seminfo.semmni;
		break;
	case SVR4_CONFIG_SEM_VALUE_MAX:
		*retval = seminfo.semvmx;
		break;
#endif
	case SVR4_CONFIG_SIGQUEUE_MAX:
		*retval = 0;	/* XXX: Don't know */
		break;
	case SVR4_CONFIG_SIGRT_MIN:
	case SVR4_CONFIG_SIGRT_MAX:
		*retval = 0;	/* No real time signals */
		break;
	case SVR4_CONFIG_TIMER_MAX:
		*retval = 3;	/* XXX: real, virtual, profiling */
		break;
	case SVR4_CONFIG_PHYS_PAGES:
		*retval = uvmexp.npages;
		break;
	case SVR4_CONFIG_AVPHYS_PAGES:
		*retval = uvmexp.active;	/* XXX: active instead of avg */
		break;
	default:
		return EINVAL;
	}
	return 0;
}

#define SVR4_RLIMIT_NOFILE	5	/* Other RLIMIT_* are the same */
#define SVR4_RLIMIT_VMEM	6	/* Other RLIMIT_* are the same */
#define SVR4_RLIM_NLIMITS	7

int
svr4_sys_getrlimit(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_getrlimit_args *uap = v;
	struct compat_43_sys_getrlimit_args ap;

	if (SCARG(uap, which) >= SVR4_RLIM_NLIMITS)
		return EINVAL;

	if (SCARG(uap, which) == SVR4_RLIMIT_NOFILE)
		SCARG(uap, which) = RLIMIT_NOFILE;
	if (SCARG(uap, which) == SVR4_RLIMIT_VMEM)
		SCARG(uap, which) = RLIMIT_RSS;

	SCARG(&ap, which) = SCARG(uap, which);
	SCARG(&ap, rlp) = SCARG(uap, rlp);

	return compat_43_sys_getrlimit(p, &ap, retval);
}

int
svr4_sys_setrlimit(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_setrlimit_args *uap = v;
	struct compat_43_sys_setrlimit_args ap;

	if (SCARG(uap, which) >= SVR4_RLIM_NLIMITS)
		return EINVAL;

	if (SCARG(uap, which) == SVR4_RLIMIT_NOFILE)
		SCARG(uap, which) = RLIMIT_NOFILE;
	if (SCARG(uap, which) == SVR4_RLIMIT_VMEM)
		SCARG(uap, which) = RLIMIT_RSS;

	SCARG(&ap, which) = SCARG(uap, which);
	SCARG(&ap, rlp) = SCARG(uap, rlp);

	return compat_43_sys_setrlimit(p, uap, retval);
}


/* ARGSUSED */
int
svr4_sys_break(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_break_args *uap = v;
	register struct vmspace *vm = p->p_vmspace;
	vaddr_t		new, old;
	int             error;
	register int    diff;

	old = (vaddr_t) vm->vm_daddr;
	new = round_page((vaddr_t)SCARG(uap, nsize));
	diff = new - old;

	DPRINTF(("break(1): old %lx new %lx diff %x\n", old, new, diff));

	if (diff > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		return ENOMEM;

	old = round_page(old + ptoa(vm->vm_dsize));
	DPRINTF(("break(2): dsize = %x ptoa %x\n",
		 vm->vm_dsize, ptoa(vm->vm_dsize)));

	diff = new - old;
	DPRINTF(("break(3): old %lx new %lx diff %x\n", old, new, diff));

	if (diff > 0) {
		error = uvm_map(&vm->vm_map, &old, diff, NULL, UVM_UNKNOWN_OFFSET,
			0, UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_COPY,
				    UVM_ADV_NORMAL,
				    UVM_FLAG_AMAPPAD|UVM_FLAG_FIXED|
				    UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW));
		if (error) {
			uprintf("sbrk: grow failed, return = %d\n", error);
			return error;
		}
		vm->vm_dsize += atop(diff);
	} else if (diff < 0) {
		diff = -diff;
		uvm_deallocate(&vm->vm_map, new, diff);
		vm->vm_dsize -= atop(diff);
	}
	return 0;
}

static __inline clock_t
timeval_to_clock_t(tv)
	struct timeval *tv;
{
	return tv->tv_sec * hz + tv->tv_usec / (1000000 / hz);
}

int
svr4_sys_times(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_times_args *uap = v;
	int			 error;
	struct tms		 tms;
	struct timeval		 t;
	struct rusage		*ru;
	struct rusage		 r;
	struct sys_getrusage_args 	 ga;

	caddr_t sg = stackgap_init(p->p_emul);
	ru = stackgap_alloc(&sg, sizeof(struct rusage));

	SCARG(&ga, who) = RUSAGE_SELF;
	SCARG(&ga, rusage) = ru;

	error = sys_getrusage(p, &ga, retval);
	if (error)
		return error;

	if ((error = copyin(ru, &r, sizeof r)) != 0)
		return error;

	tms.tms_utime = timeval_to_clock_t(&r.ru_utime);
	tms.tms_stime = timeval_to_clock_t(&r.ru_stime);

	SCARG(&ga, who) = RUSAGE_CHILDREN;
	error = sys_getrusage(p, &ga, retval);
	if (error)
		return error;

	if ((error = copyin(ru, &r, sizeof r)) != 0)
		return error;

	tms.tms_cutime = timeval_to_clock_t(&r.ru_utime);
	tms.tms_cstime = timeval_to_clock_t(&r.ru_stime);

	microtime(&t);
	*retval = timeval_to_clock_t(&t);

	return copyout(&tms, SCARG(uap, tp), sizeof(tms));
}


int
svr4_sys_ulimit(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_ulimit_args *uap = v;

	switch (SCARG(uap, cmd)) {
	case SVR4_GFILLIM:
		*retval = p->p_rlimit[RLIMIT_FSIZE].rlim_cur / 512;
		return 0;

	case SVR4_SFILLIM:
		{
			int error;
			struct sys_setrlimit_args srl;
			struct rlimit krl;
			caddr_t sg = stackgap_init(p->p_emul);
			struct rlimit *url = (struct rlimit *) 
				stackgap_alloc(&sg, sizeof *url);

			krl.rlim_cur = SCARG(uap, newlimit) * 512;
			krl.rlim_max = p->p_rlimit[RLIMIT_FSIZE].rlim_max;

			error = copyout(&krl, url, sizeof(*url));
			if (error)
				return error;

			SCARG(&srl, which) = RLIMIT_FSIZE;
			SCARG(&srl, rlp) = url;

			error = sys_setrlimit(p, &srl, retval);
			if (error)
				return error;

			*retval = p->p_rlimit[RLIMIT_FSIZE].rlim_cur;
			return 0;
		}

	case SVR4_GMEMLIM:
		{
			struct vmspace *vm = p->p_vmspace;
			*retval = (long) vm->vm_daddr +
				  p->p_rlimit[RLIMIT_DATA].rlim_cur;
			return 0;
		}

	case SVR4_GDESLIM:
		*retval = p->p_rlimit[RLIMIT_NOFILE].rlim_cur;
		return 0;

	default:
		return EINVAL;
	}
}


static struct proc *
svr4_pfind(pid)
	pid_t pid;
{
	struct proc *p;

	/* look in the live processes */
	if ((p = pfind(pid)) != NULL)
		return p;

	/* look in the zombies */
	LIST_FOREACH(p, &zombproc, p_list)
		if (p->p_pid == pid)
			return p;

	return NULL;
}


int
svr4_sys_pgrpsys(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_pgrpsys_args *uap = v;
	int error;

	switch (SCARG(uap, cmd)) {
	case 0:			/* getpgrp() */
		*retval = p->p_pgrp->pg_id;
		return 0;

	case 1:			/* setpgrp() */
		{
			struct sys_setpgid_args sa;

			SCARG(&sa, pid) = 0;
			SCARG(&sa, pgid) = 0;
			if ((error = sys_setpgid(p, &sa, retval)) != 0)
				return error;
			*retval = p->p_pgrp->pg_id;
			return 0;
		}

	case 2:			/* getsid(pid) */
		if (SCARG(uap, pid) != 0 &&
		    (p = svr4_pfind(SCARG(uap, pid))) == NULL)
			return ESRCH;
		/* 
		 * we return the pid of the session leader for this
		 * process
		 */
		*retval = (register_t) p->p_session->s_leader->p_pid;
		return 0;

	case 3:			/* setsid() */
		return sys_setsid(p, NULL, retval);

	case 4:			/* getpgid(pid) */

		if (SCARG(uap, pid) != 0 &&
		    (p = svr4_pfind(SCARG(uap, pid))) == NULL)
			return ESRCH;

		*retval = (int) p->p_pgrp->pg_id;
		return 0;

	case 5:			/* setpgid(pid, pgid); */
		{
			struct sys_setpgid_args sa;

			SCARG(&sa, pid) = SCARG(uap, pid);
			SCARG(&sa, pgid) = SCARG(uap, pgid);
			return sys_setpgid(p, &sa, retval);
		}

	default:
		return EINVAL;
	}
}

struct svr4_hrtcntl_args {
	syscallarg(int) 			cmd;
	syscallarg(int) 			fun;
	syscallarg(int) 			clk;
	syscallarg(svr4_hrt_interval_t *)	iv;
	syscallarg(svr4_hrt_time_t *)		ti;
};

static int
svr4_hrtcntl(p, uap, retval)
	register struct proc *p;
	register struct svr4_hrtcntl_args *uap;
	register_t *retval;
{
	switch (SCARG(uap, fun)) {
	case SVR4_HRT_CNTL_RES:
		DPRINTF(("htrcntl(RES)\n"));
		*retval = SVR4_HRT_USEC;
		return 0;

	case SVR4_HRT_CNTL_TOFD:
		DPRINTF(("htrcntl(TOFD)\n"));
		{
			struct timeval tv;
			svr4_hrt_time_t t;
			if (SCARG(uap, clk) != SVR4_HRT_CLK_STD) {
				DPRINTF(("clk == %d\n", SCARG(uap, clk)));
				return EINVAL;
			}
			if (SCARG(uap, ti) == NULL) {
				DPRINTF(("ti NULL\n"));
				return EINVAL;
			}
			microtime(&tv);
			t.h_sec = tv.tv_sec;
			t.h_rem = tv.tv_usec;
			t.h_res = SVR4_HRT_USEC;
			return copyout(&t, SCARG(uap, ti), sizeof(t));
		}

	case SVR4_HRT_CNTL_START:
		DPRINTF(("htrcntl(START)\n"));
		return ENOSYS;

	case SVR4_HRT_CNTL_GET:
		DPRINTF(("htrcntl(GET)\n"));
		return ENOSYS;
	default:
		DPRINTF(("Bad htrcntl command %d\n", SCARG(uap, fun)));
		return ENOSYS;
	}
}

int
svr4_sys_hrtsys(p, v, retval) 
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_hrtsys_args *uap = v;

	switch (SCARG(uap, cmd)) {
	case SVR4_HRT_CNTL:
		return svr4_hrtcntl(p, (struct svr4_hrtcntl_args *) uap,
				    retval);

	case SVR4_HRT_ALRM:
		DPRINTF(("hrtalarm\n"));
		return ENOSYS;

	case SVR4_HRT_SLP:
		DPRINTF(("hrtsleep\n"));
		return ENOSYS;

	case SVR4_HRT_CAN:
		DPRINTF(("hrtcancel\n"));
		return ENOSYS;

	default:
		DPRINTF(("Bad hrtsys command %d\n", SCARG(uap, cmd)));
		return EINVAL;
	}
}

static int
svr4_setinfo(p, st, s)
	struct proc *p;
	int st;
	svr4_siginfo_t *s;
{
	svr4_siginfo_t i;

	bzero(&i, sizeof(i));

	i.svr4_si_signo = SVR4_SIGCHLD;
	i.svr4_si_errno = 0;	/* XXX? */

	if (p) {
		i.svr4_si_pid = p->p_pid;
		if (p->p_stat == SZOMB) {
			i.svr4_si_stime = p->p_ru->ru_stime.tv_sec;
			i.svr4_si_utime = p->p_ru->ru_utime.tv_sec;
		} else {
			i.svr4_si_stime = p->p_stats->p_ru.ru_stime.tv_sec;
			i.svr4_si_utime = p->p_stats->p_ru.ru_utime.tv_sec;
		}
	}

	if (WIFEXITED(st)) {
		i.svr4_si_status = WEXITSTATUS(st);
		i.svr4_si_code = SVR4_CLD_EXITED;
	}
	else if (WIFSTOPPED(st)) {
		i.svr4_si_status = bsd_to_svr4_sig[WSTOPSIG(st)];

		if (i.svr4_si_status == SVR4_SIGCONT)
			i.svr4_si_code = SVR4_CLD_CONTINUED;
		else
			i.svr4_si_code = SVR4_CLD_STOPPED;
	} else {
		i.svr4_si_status = bsd_to_svr4_sig[WTERMSIG(st)];

		if (WCOREDUMP(st))
			i.svr4_si_code = SVR4_CLD_DUMPED;
		else
			i.svr4_si_code = SVR4_CLD_KILLED;
	}

	DPRINTF(("siginfo [pid %ld signo %d code %d errno %d status %d]\n",
		 i.svr4_si_pid, i.svr4_si_signo, i.svr4_si_code,
		 i.svr4_si_errno, i.svr4_si_status));

	return copyout(&i, s, sizeof(i));
}


int
svr4_sys_waitsys(q, v, retval) 
	struct proc *q;
	void *v;
	register_t *retval;
{
	struct svr4_sys_waitsys_args *uap = v;
	int nfound;
	int error;
	struct proc *p;

	switch (SCARG(uap, grp)) {
	case SVR4_P_PID:	
		break;

	case SVR4_P_PGID:
		SCARG(uap, id) = -q->p_pgid;
		break;

	case SVR4_P_ALL:
		SCARG(uap, id) = WAIT_ANY;
		break;

	default:
		return (EINVAL);
	}

	DPRINTF(("waitsys(%d, %d, %p, %x)\n", SCARG(uap, grp), SCARG(uap, id),
	    SCARG(uap, info), SCARG(uap, options)));

loop:
	nfound = 0;
	LIST_FOREACH(p, &q->p_children, p_sibling) {
		if (SCARG(uap, id) != WAIT_ANY &&
		    p->p_pid != SCARG(uap, id) &&
		    p->p_pgid != -SCARG(uap, id)) {
			DPRINTF(("pid %d pgid %d != %d\n", p->p_pid,
				 p->p_pgid, SCARG(uap, id)));
			continue;
		}
		nfound++;
		if (p->p_stat == SZOMB && 
		    ((SCARG(uap, options) & (SVR4_WEXITED|SVR4_WTRAPPED)))) {
			*retval = 0;
			DPRINTF(("found %d\n", p->p_pid));
			error = svr4_setinfo(p, p->p_xstat, SCARG(uap, info));
			if (error)
				return (error);

			if ((SCARG(uap, options) & SVR4_WNOWAIT)) {
				DPRINTF(("Don't wait\n"));
				return (0);
			}
			proc_finish_wait(q, p);
			return (0);
		}
		if (p->p_stat == SSTOP && (p->p_flag & P_WAITED) == 0 &&
		    (p->p_flag & P_TRACED ||
		    (SCARG(uap, options) & (SVR4_WSTOPPED|SVR4_WCONTINUED)))) {
			DPRINTF(("jobcontrol %d\n", p->p_pid));
			if (((SCARG(uap, options) & SVR4_WNOWAIT)) == 0)
				atomic_setbits_int(&p->p_flag, P_WAITED);
			*retval = 0;
			return (svr4_setinfo(p, W_STOPCODE(p->p_xstat),
			   SCARG(uap, info)));
		}
	}

	if (nfound == 0)
		return (ECHILD);

	if (SCARG(uap, options) & SVR4_WNOHANG) {
		*retval = 0;
		if ((error = svr4_setinfo(NULL, 0, SCARG(uap, info))) != 0)
			return (error);
		return (0);
	}

	if ((error = tsleep((caddr_t)q, PWAIT | PCATCH, "svr4_wait", 0)) != 0)
		return (error);
	goto loop;
}

static void
bsd_statfs_to_svr4_statvfs(bfs, sfs)
	const struct statfs *bfs;
	struct svr4_statvfs *sfs;
{
	bzero(sfs, sizeof(*sfs));
	sfs->f_bsize = bfs->f_iosize; /* XXX */
	sfs->f_frsize = bfs->f_bsize;
	sfs->f_blocks = bfs->f_blocks;
	sfs->f_bfree = bfs->f_bfree;
	sfs->f_bavail = bfs->f_bavail;
	sfs->f_files = bfs->f_files;
	sfs->f_ffree = bfs->f_ffree;
	sfs->f_favail = bfs->f_ffree;
	sfs->f_fsid = bfs->f_fsid.val[0];
	bcopy(bfs->f_fstypename, sfs->f_basetype, sizeof(sfs->f_basetype));
	if (bfs->f_flags & MNT_RDONLY)
		sfs->f_flag |= SVR4_ST_RDONLY;
	if (bfs->f_flags & MNT_NOSUID)
		sfs->f_flag |= SVR4_ST_NOSUID;
	sfs->f_namemax = MAXNAMLEN;
	bcopy(bfs->f_fstypename, sfs->f_fstr,
	    MIN(sizeof(sfs->f_fstypename), sizeof(sfs->f_fstr));
}


static void
bsd_statfs_to_svr4_statvfs64(bfs, sfs)
	const struct statfs *bfs;
	struct svr4_statvfs64 *sfs; 
{
	bzero(sfs, sizeof(*sfs));
	sfs->f_bsize = bfs->f_iosize; /* XXX */
	sfs->f_frsize = bfs->f_bsize;
	sfs->f_blocks = bfs->f_blocks;
	sfs->f_bfree = bfs->f_bfree;
	sfs->f_bavail = bfs->f_bavail;
	sfs->f_files = bfs->f_files;
	sfs->f_ffree = bfs->f_ffree;  
	sfs->f_favail = bfs->f_ffree;
	sfs->f_fsid = bfs->f_fsid.val[0];
	bcopy(bfs->f_fstypename, sfs->f_basetype, sizeof(sfs->f_basetype));
	if (bfs->f_flags & MNT_RDONLY)
		sfs->f_flag |= SVR4_ST_RDONLY;
	if (bfs->f_flags & MNT_NOSUID)
		sfs->f_flag |= SVR4_ST_NOSUID;
	sfs->f_namemax = MAXNAMLEN;   
	bcopy(bfs->f_fstypename, sfs->f_fstr,
	    MIN(sizeof(sfs->f_fstypename), sizeof(sfs->f_fstr));
}


int
svr4_sys_statvfs(p, v, retval) 
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_statvfs_args *uap = v;
	struct sys_statfs_args	fs_args;
	caddr_t sg = stackgap_init(p->p_emul);
	struct statfs *fs = stackgap_alloc(&sg, sizeof(struct statfs));
	struct statfs bfs;
	struct svr4_statvfs sfs;
	int error;

	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&fs_args, path) = SCARG(uap, path);
	SCARG(&fs_args, buf) = fs;

	if ((error = sys_statfs(p, &fs_args, retval)) != 0)
		return error;

	if ((error = copyin(fs, &bfs, sizeof(bfs))) != 0)
		return error;

	bsd_statfs_to_svr4_statvfs(&bfs, &sfs);

	return copyout(&sfs, SCARG(uap, fs), sizeof(sfs));
}


int
svr4_sys_fstatvfs(p, v, retval) 
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_fstatvfs_args *uap = v;
	struct sys_fstatfs_args	fs_args;
	caddr_t sg = stackgap_init(p->p_emul);
	struct statfs *fs = stackgap_alloc(&sg, sizeof(struct statfs));
	struct statfs bfs;
	struct svr4_statvfs sfs;
	int error;

	SCARG(&fs_args, fd) = SCARG(uap, fd);
	SCARG(&fs_args, buf) = fs;

	if ((error = sys_fstatfs(p, &fs_args, retval)) != 0)
		return error;

	if ((error = copyin(fs, &bfs, sizeof(bfs))) != 0)
		return error;

	bsd_statfs_to_svr4_statvfs(&bfs, &sfs);

	return copyout(&sfs, SCARG(uap, fs), sizeof(sfs));
}


int
svr4_sys_fstatvfs64(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_fstatvfs64_args *uap = v;
	struct sys_fstatfs_args fs_args;
	caddr_t sg = stackgap_init(p->p_emul);
	struct statfs *fs = stackgap_alloc(&sg, sizeof(struct statfs));
	struct statfs bfs;
	struct svr4_statvfs64 sfs;
	int error;

	SCARG(&fs_args, fd) = SCARG(uap, fd);
	SCARG(&fs_args, buf) = fs;

	if ((error = sys_fstatfs(p, &fs_args, retval)) != 0)
		return error;

	if ((error = copyin(fs, &bfs, sizeof(bfs))) != 0)
		return error;

	bsd_statfs_to_svr4_statvfs64(&bfs, &sfs);

	return copyout(&sfs, SCARG(uap, fs), sizeof(sfs));
}


int
svr4_sys_alarm(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_alarm_args *uap = v;
	int error;
        struct itimerval *ntp, *otp, tp;
	struct sys_setitimer_args sa;
	caddr_t sg = stackgap_init(p->p_emul);

        ntp = stackgap_alloc(&sg, sizeof(struct itimerval));
        otp = stackgap_alloc(&sg, sizeof(struct itimerval));

        timerclear(&tp.it_interval);
        tp.it_value.tv_sec = SCARG(uap, sec);
        tp.it_value.tv_usec = 0;

	if ((error = copyout(&tp, ntp, sizeof(tp))) != 0)
		return error;

	SCARG(&sa, which) = ITIMER_REAL;
	SCARG(&sa, itv) = ntp;
	SCARG(&sa, oitv) = otp;

        if ((error = sys_setitimer(p, &sa, retval)) != 0)
		return error;

	if ((error = copyin(otp, &tp, sizeof(tp))) != 0)
		return error;

        if (tp.it_value.tv_usec)
                tp.it_value.tv_sec++;

        *retval = (register_t) tp.it_value.tv_sec;

        return 0;
}


int
svr4_sys_gettimeofday(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_gettimeofday_args *uap = v;

	if (SCARG(uap, tp)) {
		struct timeval atv;

		microtime(&atv);
		return copyout(&atv, SCARG(uap, tp), sizeof (atv));
	}

	return 0;
}

int
svr4_sys_facl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_facl_args *uap = v;

	*retval = 0;

	switch (SCARG(uap, cmd)) {
	case SVR4_SYS_SETACL:
		/* We don't support acls on any filesystem */
		return ENOSYS;

	case SVR4_SYS_GETACL:
		return copyout(retval, &SCARG(uap, num),
		    sizeof(SCARG(uap, num)));

	case SVR4_SYS_GETACLCNT:
		return 0;

	default:
		return EINVAL;
	}
}

int
svr4_sys_acl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	return svr4_sys_facl(p, v, retval);	/* XXX: for now the same */
}

int
svr4_sys_auditsys(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	/*
	 * XXX: Big brother is *not* watching.
	 */
	return 0;
}

int
svr4_sys_memcntl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_memcntl_args *uap = v;
	struct sys_mprotect_args ap;

	SCARG(&ap, addr) = SCARG(uap, addr);
	SCARG(&ap, len) = SCARG(uap, len);
	SCARG(&ap, prot) = SCARG(uap, attr);

	/* XXX: no locking, invalidating, or syncing supported */
	return sys_mprotect(p, &ap, retval);
}

int
svr4_sys_nice(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_nice_args *uap = v;
	struct sys_setpriority_args ap;
	int error;

	SCARG(&ap, which) = PRIO_PROCESS;
	SCARG(&ap, who) = 0;
	SCARG(&ap, prio) = SCARG(uap, prio);

	if ((error = sys_setpriority(p, &ap, retval)) != 0)
		return error;

	if ((error = sys_getpriority(p, &ap, retval)) != 0)
		return error;

	return 0;
}

/* ARGSUSED */
int
svr4_sys_setegid(p, v, retval)
        struct proc *p;
        void *v;
        register_t *retval;
{
        struct sys_setegid_args /* {
		syscallarg(gid_t) egid;
        } */ *uap = v;

#if defined(COMPAT_LINUX) && defined(__i386__)
	if (SCARG(uap, egid) > 60000) {
		/*
		 * One great fuckup deserves another.  The Linux people
		 * made this their personality system call.  But we can't
		 * tell if a binary is SVR4 or Linux until they do that
		 * system call, in some cases.  So when we get it, and the
		 * value is out of some magical range, switch to Linux
		 * emulation and pray.
		 */
		extern struct emul emul_linux_elf;

		p->p_emul = &emul_linux_elf;
		p->p_os = OOS_LINUX;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_EMUL))
			ktremul(p, p->p_emul->e_name);
#endif
		return (0);
	}
#else
	(void)uap;
#endif
        return (sys_setegid(p, v, retval));
}

int
svr4_sys_rdebug(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#ifdef COMPAT_SVR4_NCR
	return (ENXIO);
#else
	return (p->p_os == OOS_NCR ? ENXIO : sys_nosys(p, v, retval));
#endif
}
