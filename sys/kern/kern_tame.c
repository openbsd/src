/*	$OpenBSD: kern_tame.c,v 1.5 2015/07/20 15:52:18 deraadt Exp $	*/

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 * Copyright (c) 2015 Theo de Raadt <deraadt@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/types.h>

#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/ktrace.h>

#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/mtio.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/tame.h>

#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/systm.h>

const u_int tame_syscalls[SYS_MAXSYSCALL] = {
	[SYS_exit] = 0xffffffff,

	[SYS_getuid] = _TM_SELF,
	[SYS_geteuid] = _TM_SELF,
	[SYS_getresuid] = _TM_SELF,
	[SYS_getgid] = _TM_SELF,
	[SYS_getegid] = _TM_SELF,
	[SYS_getresgid] = _TM_SELF,
	[SYS_getgroups] = _TM_SELF,
	[SYS_getlogin] = _TM_SELF,
	[SYS_getpgrp] = _TM_SELF,
	[SYS_getpgid] = _TM_SELF,
	[SYS_getppid] = _TM_SELF,
	[SYS_getsid] = _TM_SELF,
	[SYS_getthrid] = _TM_SELF,
	[SYS_getrlimit] = _TM_SELF,
	[SYS_gettimeofday] = _TM_SELF,
	[SYS_getdtablecount] = _TM_SELF,
	[SYS_issetugid] = _TM_SELF,
	[SYS_clock_getres] = _TM_SELF,
	[SYS_clock_gettime] = _TM_SELF,
	[SYS_getpid] = _TM_SELF,
	[SYS_umask] = _TM_SELF,
	[SYS___sysctl] = _TM_SELF,	/* read-only; narrow subset */
	[SYS_adjtime] = _TM_SELF,	/* read-only */

	[SYS_chdir] = _TM_RPATH,

	[SYS_fchdir] = _TM_SELF,	/* careful of directory fd inside jails */

	[SYS_sendsyslog] = _TM_SELF,
	[SYS_nanosleep] = _TM_SELF,
	[SYS_sigprocmask] = _TM_SELF,
	[SYS_sigaction] = _TM_SELF,
	[SYS_sigreturn] = _TM_SELF,
	[SYS_getitimer] = _TM_SELF,
	[SYS_setitimer] = _TM_SELF,

	[SYS_tame] = _TM_SELF,

	[SYS_wait4] = _TM_SELF,

	[SYS_poll] = _TM_RW,
	[SYS_kevent] = _TM_RW,
	[SYS_kqueue] = _TM_RW,
	[SYS_select] = _TM_RW,

	[SYS_close] = _TM_RW,
	[SYS_dup] = _TM_RW,
	[SYS_dup2] = _TM_RW,
	[SYS_dup3] = _TM_RW,
	[SYS_closefrom] = _TM_RW,
	[SYS_shutdown] = _TM_RW,
	[SYS_read] = _TM_RW,
	[SYS_readv] = _TM_RW,
	[SYS_pread] = _TM_RW,
	[SYS_preadv] = _TM_RW,
	[SYS_write] = _TM_RW,
	[SYS_writev] = _TM_RW,
	[SYS_pwrite] = _TM_RW,
	[SYS_pwritev] = _TM_RW,
	[SYS_ftruncate] = _TM_RW,
	[SYS_lseek] = _TM_RW,

	[SYS_utimes] = _TM_RW,
	[SYS_futimes] = _TM_RW,
	[SYS_utimensat] = _TM_RW,
	[SYS_futimens] = _TM_RW,

	[SYS_fcntl] = _TM_RW,
	[SYS_fsync] = _TM_RW,
	[SYS_pipe] = _TM_RW,
	[SYS_pipe2] = _TM_RW,
	[SYS_socketpair] = _TM_RW,

	[SYS_getdents] = _TM_RW,

	[SYS_sendto] = _TM_RW | _TM_DNS_ACTIVE | _TM_YP_ACTIVE,
	[SYS_sendmsg] = _TM_RW,
	[SYS_recvmsg] = _TM_RW,
	[SYS_recvfrom] = _TM_RW | _TM_DNS_ACTIVE | _TM_YP_ACTIVE,

	[SYS_fork] = _TM_PROC,
	[SYS_vfork] = _TM_PROC,
	[SYS_kill] = _TM_PROC,

	[SYS_setgroups] = _TM_PROC,
	[SYS_setresgid] = _TM_PROC,
	[SYS_setresuid] = _TM_PROC,

	[SYS_ioctl] = _TM_IOCTL,		/* very limited subset */

	[SYS_getentropy] = _TM_MALLOC,
	[SYS_madvise] = _TM_MALLOC,
	[SYS_minherit] = _TM_MALLOC,
	[SYS_mmap] = _TM_MALLOC,
	[SYS_mprotect] = _TM_MALLOC,
	[SYS_mquery] = _TM_MALLOC,
	[SYS_munmap] = _TM_MALLOC,

	[SYS___getcwd] = _TM_RPATH | _TM_WPATH,
	[SYS_open] = _TM_SELF,
	[SYS_openat] = _TM_RPATH | _TM_WPATH,
	[SYS_stat] = _TM_SELF,
	[SYS_fstatat] = _TM_RPATH | _TM_WPATH,
	[SYS_access] = _TM_SELF,
	[SYS_faccessat] = _TM_RPATH | _TM_WPATH,
	[SYS_readlink] = _TM_SELF,
	[SYS_readlinkat] = _TM_RPATH | _TM_WPATH,
	[SYS_lstat] = _TM_RPATH | _TM_WPATH | _TM_TMPPATH | _TM_DNSPATH,
	[SYS_chmod] = _TM_RPATH | _TM_WPATH | _TM_TMPPATH,
	[SYS_fchmod] = _TM_RPATH | _TM_WPATH,
	[SYS_fchmodat] = _TM_RPATH | _TM_WPATH,
	[SYS_chflags] = _TM_RPATH | _TM_WPATH | _TM_TMPPATH,
	[SYS_chflagsat] = _TM_RPATH | _TM_WPATH,
	[SYS_chown] = _TM_RPATH | _TM_WPATH | _TM_TMPPATH,
	[SYS_fchown] = _TM_RPATH | _TM_WPATH,
	[SYS_fchownat] = _TM_RPATH | _TM_WPATH,
	[SYS_rename] = _TM_CPATH,
	[SYS_rmdir] = _TM_CPATH,
	[SYS_renameat] = _TM_CPATH,
	[SYS_link] = _TM_CPATH,
	[SYS_linkat] = _TM_CPATH,
	[SYS_symlink] = _TM_CPATH,
	[SYS_unlink] = _TM_CPATH | _TM_TMPPATH,
	[SYS_unlinkat] = _TM_CPATH,
	[SYS_mkdir] = _TM_CPATH,
	[SYS_mkdirat] = _TM_CPATH,

	[SYS_fstat] = _TM_RW | _TM_RPATH | _TM_WPATH | _TM_TMPPATH,	/* rare */

	[SYS_socket] = _TM_INET | _TM_UNIX | _TM_DNS_ACTIVE | _TM_YP_ACTIVE,
	[SYS_listen] = _TM_INET | _TM_UNIX,
	[SYS_bind] = _TM_INET | _TM_UNIX,
	[SYS_connect] = _TM_INET | _TM_UNIX | _TM_DNS_ACTIVE | _TM_YP_ACTIVE,
	[SYS_accept4] = _TM_INET | _TM_UNIX,
	[SYS_accept] = _TM_INET | _TM_UNIX,
	[SYS_getpeername] = _TM_INET | _TM_UNIX,
	[SYS_getsockname] = _TM_INET | _TM_UNIX,
	[SYS_setsockopt] = _TM_INET | _TM_UNIX,		/* small subset */
	[SYS_getsockopt] = _TM_INET | _TM_UNIX,

	[SYS_flock] = _TM_GETPW,
};

int
sys_tame(struct proc *p, void *v, register_t *retval)
{
	struct sys_tame_args /* {
		syscallarg(int) flags;
	} */	*uap = v;
	int	 flags = SCARG(uap, flags);

	flags &= _TM_USERSET;
	if ((p->p_p->ps_flags & PS_TAMED) == 0) {
		p->p_p->ps_flags |= PS_TAMED;
		p->p_p->ps_tame = flags;
		return (0);
	}

	/* May not set new bits */
	if (((flags | p->p_p->ps_tame) & _TM_USERSET) !=
	    (p->p_p->ps_tame & _TM_USERSET))
		return (EPERM);

	/* More tame bits being cleared.  Force re-learning of _ACTIVE things */
	p->p_p->ps_tame &= flags;
	p->p_p->ps_tame &= _TM_USERSET;
	return (0);
}

int
tame_check(struct proc *p, int code)
{
	p->p_tamenote = p->p_tameafter = 0;	/* XX optimise? */
	p->p_tame_syscall = code;

	if (code < 0 || code > SYS_MAXSYSCALL - 1)
		return (0);

	if (p->p_p->ps_tame == 0)
		return (code == SYS_exit);
	return (p->p_p->ps_tame & tame_syscalls[code]);
}

int
tame_fail(struct proc *p, int error, int code)
{
	printf("tame: pid %d %s syscall %d\n", p->p_pid, p->p_comm,
	    p->p_tame_syscall);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_PSIG)) {
		siginfo_t si;

		memset(&si, 0, sizeof(si));
		if (p->p_p->ps_tame & _TM_ABORT)
			si.si_signo = SIGABRT;
		else
			si.si_signo = SIGKILL;
		si.si_code = code;
		// si.si_syscall = p->p_tame_syscall;
		/// si.si_nsysarg ...
		ktrpsig(p, si.si_signo, SIG_DFL, p->p_sigmask, code, &si);
	}
#endif
	if (p->p_p->ps_tame & _TM_ABORT) {
		/* Core dump requested */
		atomic_clearbits_int(&p->p_sigmask, sigmask(SIGABRT));
		atomic_clearbits_int(&p->p_p->ps_flags, PS_TAMED);
		psignal(p, SIGABRT);
	} else
		psignal(p, SIGKILL);
	return (error);
}

/*
 * Need to make it more obvious that one cannot get through here
 * without the right flags set
 */
int
tame_namei(struct proc *p, char *path)
{
	/* Detect what looks like a mkstemp(3) family operation */
	if ((p->p_p->ps_tame & _TM_TMPPATH) &&
	    (p->p_tame_syscall == SYS_open) &&
	    (p->p_tamenote & (TMN_CREAT | TMN_IMODIFY)) == TMN_CREAT &&
	    strncmp(path, "/tmp/", sizeof("/tmp/") - 1) == 0) {
		return (0);
	}

	/* Allow unlinking of a mkstemp(3) file...
	 * Good opportunity for strict checks here.
	 */
	if ((p->p_p->ps_tame & _TM_TMPPATH) &&
	    (p->p_tame_syscall == SYS_unlink) &&
	    strncmp(path, "/tmp/", sizeof("/tmp") - 1) == 0) {
		return (0);
	}

	/* open, mkdir, or other path creation operation */
	if ((p->p_tamenote & (TMN_CREAT | TMN_IMODIFY)) == TMN_CREAT &&
	    ((p->p_p->ps_tame & _TM_CPATH) == 0))
		return (tame_fail(p, EPERM, TAME_CPATH));

	/* inode change operation, issued against a path */
	if ((p->p_tamenote & (TMN_CREAT | TMN_IMODIFY)) == TMN_IMODIFY &&
	    ((p->p_p->ps_tame & _TM_CPATH) == 0)) {
		// XXX should _TM_CPATH be a seperate check?
		return (tame_fail(p, EPERM, TAME_CPATH));
	}

	if ((p->p_tamenote & TMN_WRITE) &&
	    (p->p_p->ps_tame & _TM_WPATH) == 0)
		return (tame_fail(p, EPERM, TAME_WPATH));

	if (p->p_p->ps_tame & _TM_RPATH)
		return (0);

	if (p->p_p->ps_tame & _TM_WPATH)
		return (0);

	/* All remaining cases are RPATH */
	switch (p->p_tame_syscall) {
	case SYS_access:
		/* tzset() needs this. */
		if (strcmp(path, "/etc/localtime") == 0)
			return (0);
		break;
	case SYS_open:
		/* getpw* and friends need a few files */
		if (p->p_p->ps_tame & _TM_GETPW) {
			if (strcmp(path, "/etc/spwd.db") == 0)
				return (0);
			if (strcmp(path, "/etc/pwd.db") == 0)
				return (0);
			if (strcmp(path, "/etc/group") == 0)
				return (0);
		}

		/* DNS needs /etc/{resolv.conf,hosts,services}. */
		if (p->p_p->ps_tame & _TM_DNSPATH) {
			if (strcmp(path, "/etc/resolv.conf") == 0) {
				p->p_tamenote |= TMN_DNSRESOLV;
				p->p_tameafter = 1;
				return (0);
			}
			if (strcmp(path, "/etc/hosts") == 0)
				return (0);
			if (strcmp(path, "/etc/services") == 0)
				return (0);
		}
		if (p->p_p->ps_tame & _TM_GETPW) {
			if (strcmp(path, "/var/run/ypbind.lock") == 0) {
				p->p_tamenote |= TMN_YPLOCK;
				p->p_tameafter = 1;
				return (0);
			}
			if (strncmp(path, "/var/yp/binding/",
			    sizeof("/var/yp/binding/") - 1) == 0)
				return (0);
		}
		/* tzset() needs these. */
		if (strncmp(path, "/usr/share/zoneinfo/",
		    sizeof("/usr/share/zoneinfo/") - 1) == 0)
			return (0);
		if (strcmp(path, "/etc/localtime") == 0)
			return (0);

		/* /usr/share/nls/../libc.cat returns EPERM, for strerror(3). */
		if (strncmp(path, "/usr/share/nls/",
		    sizeof("/usr/share/nls/") - 1) == 0 &&
		    strcmp(path + strlen(path) - 9, "/libc.cat") == 0)
			return (EPERM);
		break;
	case SYS_readlink:
		/* Allow /etc/malloc.conf for malloc(3). */
		if (strcmp(path, "/etc/malloc.conf") == 0)
			return (0);
		break;
	case SYS_stat:
		/* DNS needs /etc/resolv.conf. */
		if (p->p_p->ps_tame & _TM_DNSPATH) {
			if (strcmp(path, "/etc/resolv.conf") == 0) {
				p->p_tamenote |= TMN_DNSRESOLV;
				p->p_tameafter = 1;
				return (0);
			}
		}
		break;
	}

	return (tame_fail(p, EPERM, TAME_RPATH));
}

void
tame_aftersyscall(struct proc *p, int code, int error)
{
	if ((p->p_tamenote & TMN_YPLOCK) && error == 0)
		atomic_setbits_int(&p->p_p->ps_tame, _TM_YP_ACTIVE | TAME_INET);
	if ((p->p_tamenote & TMN_DNSRESOLV) && error == 0)
		atomic_setbits_int(&p->p_p->ps_tame, _TM_DNS_ACTIVE);
}

/*
 * By default, only the advisory cmsg's can be received from the kernel,
 * such as TIMESTAMP ntpd.
 *
 * If TAME_CMSG is set SCM_RIGHTS is also allowed through for a carefully
 * selected set of descriptors (specifically to exclude directories).
 *
 * This results in a kill upon recv, if some other process on the system
 * send a SCM_RIGHTS to an open socket of some sort.  That will discourage
 * leaving such sockets lying around...
 */
int
tame_cmsg_recv(struct proc *p, void *v, int controllen)
{
	struct mbuf *control = v;
	struct msghdr tmp;
	struct cmsghdr *cmsg;
	struct file **rp, *fp;
	int nfds, i;

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	/* Scan the cmsg */
	memset(&tmp, 0, sizeof(tmp));
	tmp.msg_control = mtod(control, struct cmsghdr *);
	tmp.msg_controllen = controllen;
	cmsg = CMSG_FIRSTHDR(&tmp);

	while (cmsg != NULL) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS)
			break;
		cmsg = CMSG_NXTHDR(&tmp, cmsg);
	}

	/* No SCM_RIGHTS found -> OK */
	if (cmsg == NULL)
		return (0);

	if ((p->p_p->ps_tame & _TM_CMSG) == 0)
		return tame_fail(p, EPERM, TAME_CMSG);

	/* In OpenBSD, a CMSG only contains one SCM_RIGHTS.  Check it. */ 
	rp = (struct file **)CMSG_DATA(cmsg);
	nfds = (cmsg->cmsg_len - CMSG_ALIGN(sizeof(*cmsg))) /
	    sizeof(struct file *);
	for (i = 0; i < nfds; i++) {
		struct vnode *vp;

		fp = *rp++;

		/* Only allow passing of sockets, pipes, and pure files */
		printf("f_type %d\n", fp->f_type);
		switch (fp->f_type) {
		case DTYPE_SOCKET:
		case DTYPE_PIPE:
			continue;
		case DTYPE_VNODE:
			vp = (struct vnode *)fp->f_data;
			printf("v_type %d\n", vp->v_type);
			if (vp->v_type == VREG)
				continue;
			break;
		default:
			break;
		}
		printf("bad fd type\n");
		return tame_fail(p, EPERM, TAME_CMSG);
	}
	return (0);
}

/*
 * When tamed, default prevents sending of a cmsg.
 * If CMSG flag is set, 
 */
int
tame_cmsg_send(struct proc *p, void *v, int controllen)
{
	struct mbuf *control = v;
	struct msghdr tmp;
	struct cmsghdr *cmsg;
	struct file **rp, *fp;
	int nfds, i;

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if ((p->p_p->ps_tame & _TM_CMSG) == 0)
		return tame_fail(p, EPERM, TAME_CMSG);

	/* Scan the cmsg */
	memset(&tmp, 0, sizeof(tmp));
	tmp.msg_control = mtod(control, struct cmsghdr *);
	tmp.msg_controllen = controllen;
	cmsg = CMSG_FIRSTHDR(&tmp);

	while (cmsg != NULL) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS)
			break;
		cmsg = CMSG_NXTHDR(&tmp, cmsg);
	}

	/* Contains no SCM_RIGHTS, so OK */
	if (cmsg == NULL)
		return (0);

	/* In OpenBSD, a CMSG only contains one SCM_RIGHTS.  Check it. */ 
	rp = (struct file **)CMSG_DATA(cmsg);
	nfds = (cmsg->cmsg_len - CMSG_ALIGN(sizeof(*cmsg))) /
	    sizeof(struct file *);
	for (i = 0; i < nfds; i++) {
		struct vnode *vp;

		fp = *rp++;

		/* Only allow passing of sockets, pipes, and pure files */
		printf("f_type %d\n", fp->f_type);
		switch (fp->f_type) {
		case DTYPE_SOCKET:
		case DTYPE_PIPE:
			continue;
		case DTYPE_VNODE:
			vp = (struct vnode *)fp->f_data;
			printf("v_type %d\n", vp->v_type);
			if (vp->v_type == VREG)
				continue;
			break;
		default:
			break;
		}
		/* Not allowed to send a bad fd type */
		return tame_fail(p, EPERM, TAME_CMSG);
	}
	return (0);
}

int
tame_sysctl_check(struct proc *p, int namelen, int *name, void *new)
{
	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if (new)
		return (EFAULT);

	/* getifaddrs() */
	if ((p->p_p->ps_tame & _TM_INET) &&
    	    namelen == 6 &&
	    name[0] == CTL_NET && name[1] == PF_ROUTE &&
	    name[2] == 0 && name[3] == 0 &&
	    name[4] == NET_RT_IFLIST && name[5] == 0)
		return (0);

	/* used by arp(8).  Exposes MAC addresses known on local nets */
	/* XXX Put into a special catagory. */
	if ((p->p_p->ps_tame & _TM_INET) &&
    	    namelen == 7 &&
	    name[0] == CTL_NET && name[1] == PF_ROUTE &&
	    name[2] == 0 && name[3] == AF_INET &&
	    name[4] == NET_RT_FLAGS && name[5] == RTF_LLINFO)
		return (0);

	/* used by ntpd(8) to read sensors. */
	/* XXX Put into a special catagory. */
	if (namelen >= 3 &&
	    name[0] == CTL_HW && name[1] == HW_SENSORS)
		return (0);

	/* gethostname(), getdomainname(), getpagesize() */
	if (namelen == 2 &&
	    name[0] == CTL_KERN && name[1] == KERN_HOSTNAME)
		return (0);
	if (namelen == 2 &&
	    name[0] == CTL_KERN && name[1] == KERN_DOMAINNAME)
		return (0);
	if (namelen == 2 &&
	    name[0] == CTL_HW && name[1] == HW_PAGESIZE)
		return (0);

	printf("tame: pid %d %s sysctl %d: %d %d %d %d %d %d\n",
	    p->p_pid, p->p_comm, namelen, name[0], name[1],
	    name[2], name[3], name[4], name[5]);
	return (EFAULT);
}

int
tame_adjtime_check(struct proc *p, const void *v)
{
	const struct timeval *delta = v;

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if (delta)
		return (EFAULT);
	return (0);
}

int
tame_connect_check(struct proc *p)
{
	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if ((p->p_p->ps_tame & _TM_DNS_ACTIVE))
		return (0);	/* A port check happens inside sys_connect() */

	if ((p->p_p->ps_tame & (_TM_INET | _TM_UNIX)))
		return (0);
	return (EPERM);
}

int
tame_recvfrom_check(struct proc *p, void *v)
{
	struct sockaddr *from = v;

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if ((p->p_p->ps_tame & _TM_DNS_ACTIVE) && from == NULL)
		return (0);
	if (p->p_p->ps_tame & _TM_INET)
		return (0);
	if (p->p_p->ps_tame & _TM_UNIX)
		return (0);
	if (from == NULL)
		return (0);		/* behaves just like write */
	return (EPERM);
}

int
tame_sendto_check(struct proc *p, const void *v)
{
	const struct sockaddr *to = v;
		
	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if ((p->p_p->ps_tame & _TM_DNS_ACTIVE) && to == NULL)
		return (0);

	if ((p->p_p->ps_tame & _TM_INET))
		return (0);
	if ((p->p_p->ps_tame & _TM_UNIX))
		return (0);
	if (to == NULL)
		return (0);		/* behaves just like write */
	return (EPERM);
}

int
tame_socket_check(struct proc *p, int domain)
{
	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);
	if ((p->p_p->ps_tame & (_TM_INET | _TM_UNIX)))
		return (0);
	if ((p->p_p->ps_tame & _TM_DNS_ACTIVE) && domain == AF_INET)
		return (0);
	return (EPERM);
}

int
tame_bind_check(struct proc *p, const void *v)
{

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);
	if ((p->p_p->ps_tame & _TM_INET))
		return (0);
	return (EPERM);
}

int
tame_ioctl_check(struct proc *p, long com, void *v)
{
	struct file *fp = v;
	struct vnode *vp = (struct vnode *)fp->f_data;

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	switch (com) {

	/*
	 * This is a set of "get" info ioctls at the top layer.  Hopefully
	 * a safe list, since they are used a lot.
	 */
	case FIOCLEX:
	case FIONCLEX:
	case FIONREAD:
	case FIONBIO:
	case FIOGETOWN:
		return (0);
	case FIOASYNC:
	case FIOSETOWN:
		return (EPERM);

	/* tty subsystem */
	case TIOCSWINSZ:	/* various programs */
	case TIOCSTI:		/* ksh? csh? */
		if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
			return (0);
		break;

	default:
		break;
	}

	if ((p->p_p->ps_tame & _TM_IOCTL) == 0)
		return (EPERM);

	/*
	 * Further sets of ioctl become available, but are checked a
	 * bit more carefully against the vnode.
	 */

	switch (com) {
	case BIOCGSTATS:        /* bpf: tcpdump privsep on ^C */
		if (fp->f_type == DTYPE_VNODE &&
		    fp->f_ops->fo_ioctl == vn_ioctl)
			return (0);
		break;

	case TIOCSETAF:		/* tcsetattr TCSAFLUSH, script */
		if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
			return (0);
		break;


	case MTIOCGET:
	case MTIOCTOP:
		/* for pax(1) and such, checking tapes... */
		if (fp->f_type == DTYPE_VNODE &&
		    (vp->v_type == VCHR || vp->v_type == VBLK))
			return (0);
		break;

	case SIOCGIFGROUP:
		if ((p->p_p->ps_tame & _TM_INET) &&
		    fp->f_type == DTYPE_SOCKET)
			return (0);
		break;

	default:
		printf("ioctl %lx\n", com);
		break;
	}
	return (EPERM);
}

int
tame_setsockopt_check(struct proc *p, int level, int optname)
{
	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_RTABLE:
			return (EPERM);
		}
		return (0);
	case IPPROTO_TCP: 
		switch (optname) {
		case TCP_NODELAY:
		case TCP_MD5SIG:
		case TCP_SACK_ENABLE:
		case TCP_MAXSEG:
		case TCP_NOPUSH:
			return (0);
		}
	case IPPROTO_IP:
		switch (optname) {
		case IP_TOS:
		case IP_TTL:
		case IP_MINTTL:
		case IP_PORTRANGE:
		case IP_RECVDSTADDR:
			return (0);
		}
	case IPPROTO_ICMP:
		break;
	case IPPROTO_IPV6:
	case IPPROTO_ICMPV6:
		break;
	}
	return (EPERM);
}

int
tame_dns_check(struct proc *p, in_port_t port)
{
	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if ((p->p_p->ps_tame & _TM_INET))
		return (0);
	if ((p->p_p->ps_tame & _TM_DNS_ACTIVE) && port == htons(53))
		return (0);	/* Allow a DNS connect outbound */
	return (EPERM);
}
