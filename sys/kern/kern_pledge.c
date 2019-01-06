/*	$OpenBSD: kern_pledge.c,v 1.247 2019/01/06 22:09:55 deraadt Exp $	*/

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
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>

#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/namei.h>
#include <sys/pool.h>
#include <sys/socketvar.h>
#include <sys/vnode.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/ktrace.h>
#include <sys/acct.h>

#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/mtio.h>
#include <sys/audioio.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_var.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <netinet/tcp.h>
#include <net/pfvar.h>

#include <sys/conf.h>
#include <sys/specdev.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/systm.h>

#include <dev/biovar.h>

#define PLEDGENAMES
#include <sys/pledge.h>

#include "audio.h"
#include "bpfilter.h"
#include "pf.h"
#include "pty.h"

#if defined(__amd64__) || defined(__i386__)
#include "vmm.h"
#if NVMM > 0
#include <machine/conf.h>
#endif
#endif

#if defined(__amd64__) || defined(__arm64__) || \
    defined(__i386__) || defined(__loongson__) || \
    defined(__macppc__) || defined(__sparc64__)
#include "drm.h"
#endif

uint64_t pledgereq_flags(const char *req);
int	 parsepledges(struct proc *p, const char *kname,
	    const char *promises, u_int64_t *fp);
int	 canonpath(const char *input, char *buf, size_t bufsize);
void	 unveil_destroy(struct process *ps);

/* #define DEBUG_PLEDGE */
#ifdef DEBUG_PLEDGE
int debug_pledge = 1;
#define DPRINTF(x...)    do { if (debug_pledge) printf(x); } while (0)
#define DNPRINTF(n,x...) do { if (debug_pledge >= (n)) printf(x); } while (0)
#else
#define DPRINTF(x...)
#define DNPRINTF(n,x...)
#endif

/*
 * Ordered in blocks starting with least risky and most required.
 */
const uint64_t pledge_syscalls[SYS_MAXSYSCALL] = {
	/*
	 * Minimum required
	 */
	[SYS_exit] = PLEDGE_ALWAYS,
	[SYS_kbind] = PLEDGE_ALWAYS,
	[SYS___get_tcb] = PLEDGE_ALWAYS,
	[SYS___set_tcb] = PLEDGE_ALWAYS,
	[SYS_pledge] = PLEDGE_ALWAYS,
	[SYS_sendsyslog] = PLEDGE_ALWAYS,	/* stack protector reporting */
	[SYS_thrkill] = PLEDGE_ALWAYS,		/* raise, abort, stack pro */
	[SYS_utrace] = PLEDGE_ALWAYS,		/* ltrace(1) from ld.so */

	/* "getting" information about self is considered safe */
	[SYS_getuid] = PLEDGE_STDIO,
	[SYS_geteuid] = PLEDGE_STDIO,
	[SYS_getresuid] = PLEDGE_STDIO,
	[SYS_getgid] = PLEDGE_STDIO,
	[SYS_getegid] = PLEDGE_STDIO,
	[SYS_getresgid] = PLEDGE_STDIO,
	[SYS_getgroups] = PLEDGE_STDIO,
	[SYS_getlogin_r] = PLEDGE_STDIO,
	[SYS_getpgrp] = PLEDGE_STDIO,
	[SYS_getpgid] = PLEDGE_STDIO,
	[SYS_getppid] = PLEDGE_STDIO,
	[SYS_getsid] = PLEDGE_STDIO,
	[SYS_getthrid] = PLEDGE_STDIO,
	[SYS_getrlimit] = PLEDGE_STDIO,
	[SYS_getrtable] = PLEDGE_STDIO,
	[SYS_gettimeofday] = PLEDGE_STDIO,
	[SYS_getdtablecount] = PLEDGE_STDIO,
	[SYS_getrusage] = PLEDGE_STDIO,
	[SYS_issetugid] = PLEDGE_STDIO,
	[SYS_clock_getres] = PLEDGE_STDIO,
	[SYS_clock_gettime] = PLEDGE_STDIO,
	[SYS_getpid] = PLEDGE_STDIO,

	/*
	 * Almost exclusively read-only, Very narrow subset.
	 * Use of "route", "inet", "dns", "ps", or "vminfo"
	 * expands access.
	 */
	[SYS_sysctl] = PLEDGE_STDIO,

	/* Support for malloc(3) family of operations */
	[SYS_getentropy] = PLEDGE_STDIO,
	[SYS_madvise] = PLEDGE_STDIO,
	[SYS_minherit] = PLEDGE_STDIO,
	[SYS_mmap] = PLEDGE_STDIO,
	[SYS_mprotect] = PLEDGE_STDIO,
	[SYS_mquery] = PLEDGE_STDIO,
	[SYS_munmap] = PLEDGE_STDIO,
	[SYS_msync] = PLEDGE_STDIO,
	[SYS_break] = PLEDGE_STDIO,

	[SYS_umask] = PLEDGE_STDIO,

	/* read/write operations */
	[SYS_read] = PLEDGE_STDIO,
	[SYS_readv] = PLEDGE_STDIO,
	[SYS_pread] = PLEDGE_STDIO,
	[SYS_preadv] = PLEDGE_STDIO,
	[SYS_write] = PLEDGE_STDIO,
	[SYS_writev] = PLEDGE_STDIO,
	[SYS_pwrite] = PLEDGE_STDIO,
	[SYS_pwritev] = PLEDGE_STDIO,
	[SYS_recvmsg] = PLEDGE_STDIO,
	[SYS_recvfrom] = PLEDGE_STDIO | PLEDGE_YPACTIVE,
	[SYS_ftruncate] = PLEDGE_STDIO,
	[SYS_lseek] = PLEDGE_STDIO,
	[SYS_fpathconf] = PLEDGE_STDIO,

	/*
	 * Address selection required a network pledge ("inet",
	 * "unix", "dns".
	 */
	[SYS_sendto] = PLEDGE_STDIO | PLEDGE_YPACTIVE,

	/*
	 * Address specification required a network pledge ("inet",
	 * "unix", "dns".  SCM_RIGHTS requires "sendfd" or "recvfd".
	 */
	[SYS_sendmsg] = PLEDGE_STDIO,

	/* Common signal operations */
	[SYS_nanosleep] = PLEDGE_STDIO,
	[SYS_sigaltstack] = PLEDGE_STDIO,
	[SYS_sigprocmask] = PLEDGE_STDIO,
	[SYS_sigsuspend] = PLEDGE_STDIO,
	[SYS_sigaction] = PLEDGE_STDIO,
	[SYS_sigreturn] = PLEDGE_STDIO,
	[SYS_sigpending] = PLEDGE_STDIO,
	[SYS_getitimer] = PLEDGE_STDIO,
	[SYS_setitimer] = PLEDGE_STDIO,

	/*
	 * To support event driven programming.
	 */
	[SYS_poll] = PLEDGE_STDIO,
	[SYS_ppoll] = PLEDGE_STDIO,
	[SYS_kevent] = PLEDGE_STDIO,
	[SYS_kqueue] = PLEDGE_STDIO,
	[SYS_select] = PLEDGE_STDIO,
	[SYS_pselect] = PLEDGE_STDIO,

	[SYS_fstat] = PLEDGE_STDIO,
	[SYS_fsync] = PLEDGE_STDIO,

	[SYS_setsockopt] = PLEDGE_STDIO,	/* narrow whitelist */
	[SYS_getsockopt] = PLEDGE_STDIO,	/* narrow whitelist */

	/* F_SETOWN requires PLEDGE_PROC */
	[SYS_fcntl] = PLEDGE_STDIO,

	[SYS_close] = PLEDGE_STDIO,
	[SYS_dup] = PLEDGE_STDIO,
	[SYS_dup2] = PLEDGE_STDIO,
	[SYS_dup3] = PLEDGE_STDIO,
	[SYS_closefrom] = PLEDGE_STDIO,
	[SYS_shutdown] = PLEDGE_STDIO,
	[SYS_fchdir] = PLEDGE_STDIO,	/* XXX consider tightening */

	[SYS_pipe] = PLEDGE_STDIO,
	[SYS_pipe2] = PLEDGE_STDIO,
	[SYS_socketpair] = PLEDGE_STDIO,

	[SYS_wait4] = PLEDGE_STDIO,

	/*
	 * Can kill self with "stdio".  Killing another pid
	 * requires "proc"
	 */
	[SYS_kill] = PLEDGE_STDIO,

	/*
	 * FIONREAD/FIONBIO for "stdio"
	 * Other ioctl are selectively allowed based upon other pledges.
	 */
	[SYS_ioctl] = PLEDGE_STDIO,

	/*
	 * Path access/creation calls encounter many extensive
	 * checks done during pledge_namei()
	 */
	[SYS_open] = PLEDGE_STDIO,
	[SYS_stat] = PLEDGE_STDIO,
	[SYS_access] = PLEDGE_STDIO,
	[SYS_readlink] = PLEDGE_STDIO,

	[SYS_adjtime] = PLEDGE_STDIO,   /* setting requires "settime" */
	[SYS_adjfreq] = PLEDGE_SETTIME,
	[SYS_settimeofday] = PLEDGE_SETTIME,

	/*
	 * Needed by threaded programs
	 * XXX should we have a new "threads"?
	 */
	[SYS___tfork] = PLEDGE_STDIO,
	[SYS_sched_yield] = PLEDGE_STDIO,
	[SYS_futex] = PLEDGE_STDIO,
	[SYS___thrsleep] = PLEDGE_STDIO,
	[SYS___thrwakeup] = PLEDGE_STDIO,
	[SYS___threxit] = PLEDGE_STDIO,
	[SYS___thrsigdivert] = PLEDGE_STDIO,

	[SYS_fork] = PLEDGE_PROC,
	[SYS_vfork] = PLEDGE_PROC,
	[SYS_setpgid] = PLEDGE_PROC,
	[SYS_setsid] = PLEDGE_PROC,

	[SYS_setrlimit] = PLEDGE_PROC | PLEDGE_ID,
	[SYS_getpriority] = PLEDGE_PROC | PLEDGE_ID,

	[SYS_setpriority] = PLEDGE_PROC | PLEDGE_ID,

	[SYS_setuid] = PLEDGE_ID,
	[SYS_seteuid] = PLEDGE_ID,
	[SYS_setreuid] = PLEDGE_ID,
	[SYS_setresuid] = PLEDGE_ID,
	[SYS_setgid] = PLEDGE_ID,
	[SYS_setegid] = PLEDGE_ID,
	[SYS_setregid] = PLEDGE_ID,
	[SYS_setresgid] = PLEDGE_ID,
	[SYS_setgroups] = PLEDGE_ID,
	[SYS_setlogin] = PLEDGE_ID,

	[SYS_unveil] = PLEDGE_UNVEIL,

	[SYS_execve] = PLEDGE_EXEC,

	[SYS_chdir] = PLEDGE_RPATH,
	[SYS_openat] = PLEDGE_RPATH | PLEDGE_WPATH,
	[SYS_fstatat] = PLEDGE_RPATH | PLEDGE_WPATH,
	[SYS_faccessat] = PLEDGE_RPATH | PLEDGE_WPATH,
	[SYS_readlinkat] = PLEDGE_RPATH | PLEDGE_WPATH,
	[SYS_lstat] = PLEDGE_RPATH | PLEDGE_WPATH | PLEDGE_TMPPATH,
	[SYS_truncate] = PLEDGE_WPATH,
	[SYS_rename] = PLEDGE_RPATH | PLEDGE_CPATH,
	[SYS_rmdir] = PLEDGE_CPATH,
	[SYS_renameat] = PLEDGE_CPATH,
	[SYS_link] = PLEDGE_CPATH,
	[SYS_linkat] = PLEDGE_CPATH,
	[SYS_symlink] = PLEDGE_CPATH,
	[SYS_symlinkat] = PLEDGE_CPATH,
	[SYS_unlink] = PLEDGE_CPATH | PLEDGE_TMPPATH,
	[SYS_unlinkat] = PLEDGE_CPATH,
	[SYS_mkdir] = PLEDGE_CPATH,
	[SYS_mkdirat] = PLEDGE_CPATH,

	[SYS_mkfifo] = PLEDGE_DPATH,
	[SYS_mknod] = PLEDGE_DPATH,

	[SYS_revoke] = PLEDGE_TTY,	/* also requires PLEDGE_RPATH */

	/*
	 * Classify as RPATH|WPATH, because of path information leakage.
	 * WPATH due to unknown use of mk*temp(3) on non-/tmp paths..
	 */
	[SYS___getcwd] = PLEDGE_RPATH | PLEDGE_WPATH,

	/* Classify as RPATH, because these leak path information */
	[SYS_getdents] = PLEDGE_RPATH,
	[SYS_getfsstat] = PLEDGE_RPATH,
	[SYS_statfs] = PLEDGE_RPATH,
	[SYS_fstatfs] = PLEDGE_RPATH,
	[SYS_pathconf] = PLEDGE_RPATH,

	[SYS_utimes] = PLEDGE_FATTR,
	[SYS_futimes] = PLEDGE_FATTR,
	[SYS_utimensat] = PLEDGE_FATTR,
	[SYS_futimens] = PLEDGE_FATTR,
	[SYS_chmod] = PLEDGE_FATTR,
	[SYS_fchmod] = PLEDGE_FATTR,
	[SYS_fchmodat] = PLEDGE_FATTR,
	[SYS_chflags] = PLEDGE_FATTR,
	[SYS_chflagsat] = PLEDGE_FATTR,
	[SYS_fchflags] = PLEDGE_FATTR,

	[SYS_chown] = PLEDGE_CHOWN,
	[SYS_fchownat] = PLEDGE_CHOWN,
	[SYS_lchown] = PLEDGE_CHOWN,
	[SYS_fchown] = PLEDGE_CHOWN,

	[SYS_socket] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS | PLEDGE_YPACTIVE,
	[SYS_connect] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS | PLEDGE_YPACTIVE,
	[SYS_bind] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS | PLEDGE_YPACTIVE,
	[SYS_getsockname] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS | PLEDGE_YPACTIVE,

	[SYS_listen] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_accept4] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_accept] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_getpeername] = PLEDGE_INET | PLEDGE_UNIX,

	[SYS_flock] = PLEDGE_FLOCK | PLEDGE_YPACTIVE,

	[SYS_swapctl] = PLEDGE_VMINFO,	/* XXX should limit to "get" operations */
};

static const struct {
	char *name;
	uint64_t flags;
} pledgereq[] = {
	{ "audio",		PLEDGE_AUDIO },
	{ "bpf",		PLEDGE_BPF },
	{ "chown",		PLEDGE_CHOWN | PLEDGE_CHOWNUID },
	{ "cpath",		PLEDGE_CPATH },
	{ "disklabel",		PLEDGE_DISKLABEL },
	{ "dns",		PLEDGE_DNS },
	{ "dpath",		PLEDGE_DPATH },
	{ "drm",		PLEDGE_DRM },
	{ "error",		PLEDGE_ERROR },
	{ "exec",		PLEDGE_EXEC },
	{ "fattr",		PLEDGE_FATTR | PLEDGE_CHOWN },
	{ "flock",		PLEDGE_FLOCK },
	{ "getpw",		PLEDGE_GETPW },
	{ "id",			PLEDGE_ID },
	{ "inet",		PLEDGE_INET },
	{ "mcast",		PLEDGE_MCAST },
	{ "pf",			PLEDGE_PF },
	{ "proc",		PLEDGE_PROC },
	{ "prot_exec",		PLEDGE_PROTEXEC },
	{ "ps",			PLEDGE_PS },
	{ "recvfd",		PLEDGE_RECVFD },
	{ "route",		PLEDGE_ROUTE },
	{ "rpath",		PLEDGE_RPATH },
	{ "sendfd",		PLEDGE_SENDFD },
	{ "settime",		PLEDGE_SETTIME },
	{ "stdio",		PLEDGE_STDIO },
	{ "tape",		PLEDGE_TAPE },
	{ "tmppath",		PLEDGE_TMPPATH },
	{ "tty",		PLEDGE_TTY },
	{ "unix",		PLEDGE_UNIX },
	{ "unveil",		PLEDGE_UNVEIL },
	{ "vminfo",		PLEDGE_VMINFO },
	{ "vmm",		PLEDGE_VMM },
	{ "wpath",		PLEDGE_WPATH },
	{ "wroute",		PLEDGE_WROUTE },
};

int
parsepledges(struct proc *p, const char *kname, const char *promises, u_int64_t *fp)
{
	size_t rbuflen;
	char *rbuf, *rp, *pn;
	u_int64_t flags = 0, f;
	int error;

	rbuf = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	error = copyinstr(promises, rbuf, MAXPATHLEN,
	    &rbuflen);
	if (error) {
		free(rbuf, M_TEMP, MAXPATHLEN);
		return (error);
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_STRUCT))
		ktrstruct(p, kname, rbuf, rbuflen-1);
#endif

	for (rp = rbuf; rp && *rp; rp = pn) {
		pn = strchr(rp, ' ');	/* find terminator */
		if (pn) {
			while (*pn == ' ')
				*pn++ = '\0';
		}
		if ((f = pledgereq_flags(rp)) == 0) {
			free(rbuf, M_TEMP, MAXPATHLEN);
			return (EINVAL);
		}
		flags |= f;
	}
	free(rbuf, M_TEMP, MAXPATHLEN);
	*fp = flags;
	return 0;
}

int
sys_pledge(struct proc *p, void *v, register_t *retval)
{
	struct sys_pledge_args /* {
		syscallarg(const char *)promises;
		syscallarg(const char *)execpromises;
	} */	*uap = v;
	struct process *pr = p->p_p;
	uint64_t promises, execpromises;
	int error;

	if (SCARG(uap, promises)) {
		error = parsepledges(p, "pledgereq",
		    SCARG(uap, promises), &promises);
		if (error)
			return (error);

		/* In "error" mode, ignore promise increase requests,
		 * but accept promise decrease requests */
		if (ISSET(pr->ps_flags, PS_PLEDGE) &&
		    (pr->ps_pledge & PLEDGE_ERROR))
			promises &= (pr->ps_pledge & PLEDGE_USERSET);

		/* Only permit reductions */
		if (ISSET(pr->ps_flags, PS_PLEDGE) &&
		    (((promises | pr->ps_pledge) != pr->ps_pledge)))
			return (EPERM);
	}
	if (SCARG(uap, execpromises)) {
		error = parsepledges(p, "pledgeexecreq",
		    SCARG(uap, execpromises), &execpromises);
		if (error)
			return (error);

		/* Only permit reductions */
		if (ISSET(pr->ps_flags, PS_EXECPLEDGE) &&
		    (((execpromises | pr->ps_execpledge) != pr->ps_execpledge)))
			return (EPERM);
	}

	if (SCARG(uap, promises)) {
		pr->ps_pledge = promises;
		pr->ps_flags |= PS_PLEDGE;
		/*
		 * Kill off unveil and drop unveil vnode refs if we no
		 * longer are holding any path-accessing pledge
		 */
		if ((pr->ps_pledge & (PLEDGE_RPATH | PLEDGE_WPATH |
		    PLEDGE_CPATH | PLEDGE_DPATH | PLEDGE_TMPPATH | PLEDGE_EXEC |
		    PLEDGE_UNIX | PLEDGE_UNVEIL)) == 0)
			unveil_destroy(pr);
	}
	if (SCARG(uap, execpromises)) {
		pr->ps_execpledge = execpromises;
		pr->ps_flags |= PS_EXECPLEDGE;
	}
	return (0);
}

int
pledge_syscall(struct proc *p, int code, uint64_t *tval)
{
	p->p_pledge_syscall = code;
	*tval = 0;

	if (code < 0 || code > SYS_MAXSYSCALL - 1)
		return (EINVAL);

	if (pledge_syscalls[code] == PLEDGE_ALWAYS)
		return (0);

	if (p->p_p->ps_pledge & pledge_syscalls[code])
		return (0);

	*tval = pledge_syscalls[code];
	return (EPERM);
}

int
pledge_fail(struct proc *p, int error, uint64_t code)
{
	char *codes = "";
	int i;
	struct sigaction sa;

	/* Print first matching pledge */
	for (i = 0; code && pledgenames[i].bits != 0; i++)
		if (pledgenames[i].bits & code) {
			codes = pledgenames[i].name;
			break;
		}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_PLEDGE))
		ktrpledge(p, error, code, p->p_pledge_syscall);
#endif
	if (p->p_p->ps_pledge & PLEDGE_ERROR)
		return (ENOSYS);

	KERNEL_LOCK();
	log(LOG_ERR, "%s[%d]: pledge \"%s\", syscall %d\n",
	    p->p_p->ps_comm, p->p_p->ps_pid, codes, p->p_pledge_syscall);
	p->p_p->ps_acflag |= APLEDGE;

	/* Send uncatchable SIGABRT for coredump */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_DFL;
	setsigvec(p, SIGABRT, &sa);
	atomic_clearbits_int(&p->p_sigmask, sigmask(SIGABRT));
	psignal(p, SIGABRT);

	p->p_p->ps_pledge = 0;		/* Disable all PLEDGE_ flags */
	KERNEL_UNLOCK();
	return (error);
}

/*
 * Need to make it more obvious that one cannot get through here
 * without the right flags set
 */
int
pledge_namei(struct proc *p, struct nameidata *ni, char *origpath)
{
	char path[PATH_MAX];
	int error;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0 ||
	    (p->p_p->ps_flags & PS_COREDUMP))
		return (0);

	if (ni->ni_pledge == 0)
		panic("pledge_namei: ni_pledge");

	/*
	 * We set the BYPASSUNVEIL flag to skip unveil checks
	 * as necessary
	 */

	/* Doing a permitted execve() */
	if ((ni->ni_pledge & PLEDGE_EXEC) &&
	    (p->p_p->ps_pledge & PLEDGE_EXEC))
		return (0);

	error = canonpath(origpath, path, sizeof(path));
	if (error)
		return (error);

	/* Detect what looks like a mkstemp(3) family operation */
	if ((p->p_p->ps_pledge & PLEDGE_TMPPATH) &&
	    (p->p_pledge_syscall == SYS_open) &&
	    (ni->ni_pledge & PLEDGE_CPATH) &&
	    strncmp(path, "/tmp/", sizeof("/tmp/") - 1) == 0) {
		ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
		return (0);
	}

	/* Allow unlinking of a mkstemp(3) file...
	 * Good opportunity for strict checks here.
	 */
	if ((p->p_p->ps_pledge & PLEDGE_TMPPATH) &&
	    (p->p_pledge_syscall == SYS_unlink) &&
	    strncmp(path, "/tmp/", sizeof("/tmp/") - 1) == 0) {
		ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
		return (0);
	}

	/* Whitelisted paths */
	switch (p->p_pledge_syscall) {
	case SYS_access:
		/* tzset() needs this. */
		if (ni->ni_pledge == PLEDGE_RPATH &&
		    strcmp(path, "/etc/localtime") == 0) {
			ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
			return (0);
		}

		/* when avoiding YP mode, getpw* functions touch this */
		if (ni->ni_pledge == PLEDGE_RPATH &&
		    strcmp(path, "/var/run/ypbind.lock") == 0) {
			if ((p->p_p->ps_pledge & PLEDGE_GETPW) ||
			    (ni->ni_unveil == UNVEIL_INSPECT)) {
				ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
				return (0);
			} else
				return (pledge_fail(p, error, PLEDGE_GETPW));
		}
		break;
	case SYS_open:
		/* daemon(3) or other such functions */
		if ((ni->ni_pledge & ~(PLEDGE_RPATH | PLEDGE_WPATH)) == 0 &&
		    strcmp(path, "/dev/null") == 0) {
			ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
			return (0);
		}

		/* readpassphrase(3), getpass(3) */
		if ((p->p_p->ps_pledge & PLEDGE_TTY) &&
		    (ni->ni_pledge & ~(PLEDGE_RPATH | PLEDGE_WPATH)) == 0 &&
		    strcmp(path, "/dev/tty") == 0) {
			ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
			return (0);
		}

		/* getpw* and friends need a few files */
		if ((ni->ni_pledge == PLEDGE_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_GETPW)) {
			if (strcmp(path, "/etc/spwd.db") == 0)
				return (EPERM); /* don't call pledge_fail */
			if (strcmp(path, "/etc/pwd.db") == 0) {
				ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
				return (0);
			}
			if (strcmp(path, "/etc/group") == 0) {
				ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
				return (0);
			}
			if (strcmp(path, "/etc/netid") == 0) {
				ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
				return (0);
			}
		}

		/* DNS needs /etc/{resolv.conf,hosts,services}. */
		if ((ni->ni_pledge == PLEDGE_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_DNS)) {
			if (strcmp(path, "/etc/resolv.conf") == 0) {
				ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
				return (0);
			}
			if (strcmp(path, "/etc/hosts") == 0) {
				ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
				return (0);
			}
			if (strcmp(path, "/etc/services") == 0) {
				ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
				return (0);
			}
		}

		if ((ni->ni_pledge == PLEDGE_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_GETPW)) {
			if (strcmp(path, "/var/run/ypbind.lock") == 0) {
				/*
				 * XXX
				 * The current hack for YP support in "getpw"
				 * is to enable some "inet" features until
				 * next pledge call.  This is not considered
				 * worse than pre-pledge, but is a work in
				 * progress, needing a clever design.
				 */
				p->p_p->ps_pledge |= PLEDGE_YPACTIVE;
				ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
				return (0);
			}
			if (strncmp(path, "/var/yp/binding/",
			    sizeof("/var/yp/binding/") - 1) == 0) {
				ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
				return (0);
			}
		}

		/* tzset() needs these. */
		if ((ni->ni_pledge == PLEDGE_RPATH) &&
		    strncmp(path, "/usr/share/zoneinfo/",
		    sizeof("/usr/share/zoneinfo/") - 1) == 0)  {
			ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
			return (0);
		}
		if ((ni->ni_pledge == PLEDGE_RPATH) &&
		    strcmp(path, "/etc/localtime") == 0) {
			ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
			return (0);
		}

		break;
	case SYS_readlink:
		/* Allow /etc/malloc.conf for malloc(3). */
		if ((ni->ni_pledge == PLEDGE_RPATH) &&
		    strcmp(path, "/etc/malloc.conf") == 0) {
			ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
			return (0);
		}
		break;
	case SYS_stat:
		/* DNS needs /etc/resolv.conf. */
		if ((ni->ni_pledge == PLEDGE_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_DNS) &&
		    strcmp(path, "/etc/resolv.conf") == 0) {
			ni->ni_cnd.cn_flags |= BYPASSUNVEIL;
			return (0);
		}
		break;
	}

	/*
	 * Ensure each flag of ni_pledge has counterpart allowing it in
	 * ps_pledge.
	 */
	if (ni->ni_pledge & ~p->p_p->ps_pledge)
		return (pledge_fail(p, EPERM, (ni->ni_pledge & ~p->p_p->ps_pledge)));

	/* continue, and check unveil if present */
	return (0);
}

/*
 * Only allow reception of safe file descriptors.
 */
int
pledge_recvfd(struct proc *p, struct file *fp)
{
	struct vnode *vp;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);
	if ((p->p_p->ps_pledge & PLEDGE_RECVFD) == 0)
		return pledge_fail(p, EPERM, PLEDGE_RECVFD);

	switch (fp->f_type) {
	case DTYPE_SOCKET:
	case DTYPE_PIPE:
	case DTYPE_DMABUF:
		return (0);
	case DTYPE_VNODE:
		vp = fp->f_data;

		if (vp->v_type != VDIR)
			return (0);
	}
	return pledge_fail(p, EINVAL, PLEDGE_RECVFD);
}

/*
 * Only allow sending of safe file descriptors.
 */
int
pledge_sendfd(struct proc *p, struct file *fp)
{
	struct vnode *vp;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);
	if ((p->p_p->ps_pledge & PLEDGE_SENDFD) == 0)
		return pledge_fail(p, EPERM, PLEDGE_SENDFD);

	switch (fp->f_type) {
	case DTYPE_SOCKET:
	case DTYPE_PIPE:
	case DTYPE_DMABUF:
		return (0);
	case DTYPE_VNODE:
		vp = fp->f_data;

		if (vp->v_type != VDIR)
			return (0);
		break;
	}
	return pledge_fail(p, EINVAL, PLEDGE_SENDFD);
}

int
pledge_sysctl(struct proc *p, int miblen, int *mib, void *new)
{
	char	buf[80];
	int	i;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if (new)
		return pledge_fail(p, EFAULT, 0);

	/* routing table observation */
	if ((p->p_p->ps_pledge & PLEDGE_ROUTE)) {
		if ((miblen == 6 || miblen == 7) &&
		    mib[0] == CTL_NET && mib[1] == PF_ROUTE &&
		    mib[2] == 0 &&
		    mib[4] == NET_RT_DUMP)
			return (0);

		if (miblen == 6 &&
		    mib[0] == CTL_NET && mib[1] == PF_ROUTE &&
		    mib[2] == 0 &&
		    (mib[3] == 0 || mib[3] == AF_INET6 || mib[3] == AF_INET) &&
		    mib[4] == NET_RT_TABLE)
			return (0);

		if (miblen == 7 &&		/* exposes MACs */
		    mib[0] == CTL_NET && mib[1] == PF_ROUTE &&
		    mib[2] == 0 &&
		    (mib[3] == 0 || mib[3] == AF_INET6 || mib[3] == AF_INET) &&
		    mib[4] == NET_RT_FLAGS && mib[5] == RTF_LLINFO)
			return (0);
	}

	if ((p->p_p->ps_pledge & PLEDGE_WROUTE)) {
		if (miblen == 4 &&
		    mib[0] == CTL_NET && mib[1] == PF_INET6 &&
		    mib[2] == IPPROTO_IPV6 && mib[3] == IPV6CTL_SOIIKEY)
			return (0);
	}

	if (p->p_p->ps_pledge & (PLEDGE_PS | PLEDGE_VMINFO)) {
		if (miblen == 2 &&		/* kern.fscale */
		    mib[0] == CTL_KERN && mib[1] == KERN_FSCALE)
			return (0);
		if (miblen == 2 &&		/* kern.boottime */
		    mib[0] == CTL_KERN && mib[1] == KERN_BOOTTIME)
			return (0);
		if (miblen == 2 &&		/* kern.consdev */
		    mib[0] == CTL_KERN && mib[1] == KERN_CONSDEV)
			return (0);
		if (miblen == 2 &&			/* kern.cptime */
		    mib[0] == CTL_KERN && mib[1] == KERN_CPTIME)
			return (0);
		if (miblen == 3 &&			/* kern.cptime2 */
		    mib[0] == CTL_KERN && mib[1] == KERN_CPTIME2)
			return (0);
		if (miblen == 3 &&			/* kern.cpustats */
		    mib[0] == CTL_KERN && mib[1] == KERN_CPUSTATS)
			return (0);
	}

	if ((p->p_p->ps_pledge & PLEDGE_PS)) {
		if (miblen == 4 &&		/* kern.procargs.* */
		    mib[0] == CTL_KERN && mib[1] == KERN_PROC_ARGS &&
		    (mib[3] == KERN_PROC_ARGV || mib[3] == KERN_PROC_ENV))
			return (0);
		if (miblen == 6 &&		/* kern.proc.* */
		    mib[0] == CTL_KERN && mib[1] == KERN_PROC)
			return (0);
		if (miblen == 3 &&		/* kern.proc_cwd.* */
		    mib[0] == CTL_KERN && mib[1] == KERN_PROC_CWD)
			return (0);
		if (miblen == 2 &&		/* hw.physmem */
		    mib[0] == CTL_HW && mib[1] == HW_PHYSMEM64)
			return (0);
		if (miblen == 2 &&		/* kern.ccpu */
		    mib[0] == CTL_KERN && mib[1] == KERN_CCPU)
			return (0);
		if (miblen == 2 &&		/* vm.maxslp */
		    mib[0] == CTL_VM && mib[1] == VM_MAXSLP)
			return (0);
	}

	if ((p->p_p->ps_pledge & PLEDGE_VMINFO)) {
		if (miblen == 2 &&		/* vm.uvmexp */
		    mib[0] == CTL_VM && mib[1] == VM_UVMEXP)
			return (0);
		if (miblen == 3 &&		/* vfs.generic.bcachestat */
		    mib[0] == CTL_VFS && mib[1] == VFS_GENERIC &&
		    mib[2] == VFS_BCACHESTAT)
			return (0);
	}

	if ((p->p_p->ps_pledge & (PLEDGE_ROUTE | PLEDGE_INET | PLEDGE_DNS))) {
		if (miblen == 6 &&		/* getifaddrs() */
		    mib[0] == CTL_NET && mib[1] == PF_ROUTE &&
		    mib[2] == 0 &&
		    (mib[3] == 0 || mib[3] == AF_INET6 || mib[3] == AF_INET) &&
		    mib[4] == NET_RT_IFLIST)
			return (0);
	}

	if ((p->p_p->ps_pledge & PLEDGE_DISKLABEL)) {
		if (miblen == 2 &&		/* kern.rawpartition */
		    mib[0] == CTL_KERN &&
		    mib[1] == KERN_RAWPARTITION)
			return (0);
		if (miblen == 2 &&		/* kern.maxpartitions */
		    mib[0] == CTL_KERN &&
		    mib[1] == KERN_MAXPARTITIONS)
			return (0);
#ifdef CPU_CHR2BLK
		if (miblen == 3 &&		/* machdep.chr2blk */
		    mib[0] == CTL_MACHDEP &&
		    mib[1] == CPU_CHR2BLK)
			return (0);
#endif /* CPU_CHR2BLK */
	}

	if (miblen >= 3 &&			/* ntpd(8) to read sensors */
	    mib[0] == CTL_HW && mib[1] == HW_SENSORS)
		return (0);
	
	if (miblen == 6 &&		/* if_nameindex() */
	    mib[0] == CTL_NET && mib[1] == PF_ROUTE &&
	    mib[2] == 0 && mib[3] == 0 && mib[4] == NET_RT_IFNAMES)
		return (0);

	if (miblen == 2) {
		switch (mib[0]) {
		case CTL_KERN:
			switch (mib[1]) {
			case KERN_DOMAINNAME:	/* getdomainname() */
			case KERN_HOSTNAME:	/* gethostname() */
			case KERN_OSTYPE:	/* uname() */
			case KERN_OSRELEASE:	/* uname() */
			case KERN_OSVERSION:	/* uname() */
			case KERN_VERSION:	/* uname() */
			case KERN_CLOCKRATE:	/* kern.clockrate */
			case KERN_ARGMAX:	/* kern.argmax */
			case KERN_NGROUPS:	/* kern.ngroups */
			case KERN_SYSVSHM:	/* kern.sysvshm */
			case KERN_POSIX1:	/* kern.posix1version */
				return (0);
			}
			break;
		case CTL_HW:
			switch (mib[1]) {
			case HW_MACHINE: 	/* uname() */
			case HW_PAGESIZE: 	/* getpagesize() */
			case HW_NCPU:		/* hw.ncpu */
			case HW_NCPUONLINE:	/* hw.ncpuonline */
				return (0);
			}
			break;
		case CTL_VM:
			switch (mib[1]) {
			case VM_PSSTRINGS:	/* setproctitle() */
			case VM_LOADAVG:	/* vm.loadavg / getloadavg(3) */
			case VM_MALLOC_CONF:	/* vm.malloc_conf */
				return (0);
			}
			break;
		default:
			break;
		}
	}

#ifdef CPU_SSE
	if (miblen == 2 &&		/* i386 libm tests for SSE */
	    mib[0] == CTL_MACHDEP && mib[1] == CPU_SSE)
		return (0);
#endif /* CPU_SSE */

	snprintf(buf, sizeof(buf), "%s(%d): pledge sysctl %d:",
	    p->p_p->ps_comm, p->p_p->ps_pid, miblen);
	for (i = 0; i < miblen; i++) {
		char *p = buf + strlen(buf);
		snprintf(p, sizeof(buf) - (p - buf), " %d", mib[i]);
	}
	log(LOG_ERR, "%s\n", buf);

	return pledge_fail(p, EINVAL, 0);
}

int
pledge_chown(struct proc *p, uid_t uid, gid_t gid)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if (p->p_p->ps_pledge & PLEDGE_CHOWNUID)
		return (0);

	if (uid != -1 && uid != p->p_ucred->cr_uid)
		return (EPERM);
	if (gid != -1 && !groupmember(gid, p->p_ucred))
		return (EPERM);
	return (0);
}

int
pledge_adjtime(struct proc *p, const void *v)
{
	const struct timeval *delta = v;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_SETTIME))
		return (0);
	if (delta)
		return (EPERM);
	return (0);
}

int
pledge_sendit(struct proc *p, const void *to)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & (PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS | PLEDGE_YPACTIVE)))
		return (0);		/* may use address */
	if (to == NULL)
		return (0);		/* behaves just like write */
	return pledge_fail(p, EPERM, PLEDGE_INET);
}

int
pledge_ioctl(struct proc *p, long com, struct file *fp)
{
	struct vnode *vp = NULL;
	int error = EPERM;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	/*
	 * The ioctl's which are always allowed.
	 */
	switch (com) {
	case FIONREAD:
	case FIONBIO:
	case FIOCLEX:
	case FIONCLEX:
		return (0);
	}

	/* fp != NULL was already checked */
	if (fp->f_type == DTYPE_VNODE) {
		vp = fp->f_data;
		if (vp->v_type == VBAD)
			return (ENOTTY);
	}

	if ((p->p_p->ps_pledge & PLEDGE_INET)) {
		switch (com) {
		case SIOCATMARK:
		case SIOCGIFGROUP:
			if (fp->f_type == DTYPE_SOCKET)
				return (0);
			break;
		}
	}

#if NBPFILTER > 0
	if ((p->p_p->ps_pledge & PLEDGE_BPF)) {
		switch (com) {
		case BIOCGSTATS:	/* bpf: tcpdump privsep on ^C */
			if (fp->f_type == DTYPE_VNODE &&
			    fp->f_ops->fo_ioctl == vn_ioctl &&
			    vp->v_type == VCHR &&
			    cdevsw[major(vp->v_rdev)].d_open == bpfopen)
				return (0);
			break;
		}
	}
#endif /* NBPFILTER > 0 */

	if ((p->p_p->ps_pledge & PLEDGE_TAPE)) {
		switch (com) {
		case MTIOCGET:
		case MTIOCTOP:
			/* for pax(1) and such, checking tapes... */
			if (fp->f_type == DTYPE_VNODE &&
			    vp->v_type == VCHR) {
				if (vp->v_flag & VISTTY)
					return (ENOTTY);
				else
					return (0);
			}
			break;
		}
	}

#if NDRM > 0
	if ((p->p_p->ps_pledge & PLEDGE_DRM)) {
		if ((fp->f_type == DTYPE_VNODE) &&
		    (vp->v_type == VCHR) &&
		    (cdevsw[major(vp->v_rdev)].d_open == drmopen)) {
			error = pledge_ioctl_drm(p, com, vp->v_rdev);
			if (error == 0)
				return 0;
		}
	}
#endif /* NDRM > 0 */

#if NAUDIO > 0
	if ((p->p_p->ps_pledge & PLEDGE_AUDIO)) {
		switch (com) {
		case AUDIO_GETPOS:
		case AUDIO_GETPAR:
		case AUDIO_SETPAR:
		case AUDIO_START:
		case AUDIO_STOP:
			if (fp->f_type == DTYPE_VNODE &&
			    vp->v_type == VCHR &&
			    cdevsw[major(vp->v_rdev)].d_open == audioopen)
				return (0);
		}
	}
#endif /* NAUDIO > 0 */

	if ((p->p_p->ps_pledge & PLEDGE_DISKLABEL)) {
		switch (com) {
		case DIOCGDINFO:
		case DIOCGPDINFO:
		case DIOCRLDINFO:
		case DIOCWDINFO:
		case BIOCDISK:
		case BIOCINQ:
		case BIOCINSTALLBOOT:
		case BIOCVOL:
			if (fp->f_type == DTYPE_VNODE &&
			    ((vp->v_type == VCHR &&
			    cdevsw[major(vp->v_rdev)].d_type == D_DISK) ||
			    (vp->v_type == VBLK &&
			    bdevsw[major(vp->v_rdev)].d_type == D_DISK)))
				return (0);
			break;
		case DIOCMAP:
			if (fp->f_type == DTYPE_VNODE &&
			    vp->v_type == VCHR &&
			    cdevsw[major(vp->v_rdev)].d_ioctl == diskmapioctl)
				return (0);
			break;
		}
	}

#if NPF > 0
	if ((p->p_p->ps_pledge & PLEDGE_PF)) {
		switch (com) {
		case DIOCADDRULE:
		case DIOCGETSTATUS:
		case DIOCNATLOOK:
		case DIOCRADDTABLES:
		case DIOCRCLRADDRS:
		case DIOCRCLRTABLES:
		case DIOCRCLRTSTATS:
		case DIOCRGETTSTATS:
		case DIOCRSETADDRS:
		case DIOCXBEGIN:
		case DIOCXCOMMIT:
		case DIOCKILLSRCNODES:
			if ((fp->f_type == DTYPE_VNODE) &&
			    (vp->v_type == VCHR) &&
			    (cdevsw[major(vp->v_rdev)].d_open == pfopen))
				return (0);
			break;
		}
	}
#endif

	if ((p->p_p->ps_pledge & PLEDGE_TTY)) {
		switch (com) {
#if NPTY > 0
		case PTMGET:
			if ((p->p_p->ps_pledge & PLEDGE_RPATH) == 0)
				break;
			if ((p->p_p->ps_pledge & PLEDGE_WPATH) == 0)
				break;
			if (fp->f_type != DTYPE_VNODE || vp->v_type != VCHR)
				break;
			if (cdevsw[major(vp->v_rdev)].d_open != ptmopen)
				break;
			return (0);
		case TIOCUCNTL:		/* vmd */
			if ((p->p_p->ps_pledge & PLEDGE_RPATH) == 0)
				break;
			if ((p->p_p->ps_pledge & PLEDGE_WPATH) == 0)
				break;
			if (cdevsw[major(vp->v_rdev)].d_open != ptcopen)
				break;
			return (0);
#endif /* NPTY > 0 */
		case TIOCSPGRP:
			if ((p->p_p->ps_pledge & PLEDGE_PROC) == 0)
				break;
			/* FALLTHROUGH */
		case TIOCFLUSH:		/* getty, telnet */
		case TIOCSTART:		/* emacs, etc */
		case TIOCGPGRP:
		case TIOCGETA:
		case TIOCGWINSZ:	/* ENOTTY return for non-tty */
		case TIOCSTAT:		/* csh */
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			return (ENOTTY);
		case TIOCSWINSZ:
		case TIOCEXT:		/* mail, libedit .. */
		case TIOCCBRK:		/* cu */
		case TIOCSBRK:		/* cu */
		case TIOCCDTR:		/* cu */
		case TIOCSDTR:		/* cu */
		case TIOCEXCL:		/* cu */
		case TIOCSETA:		/* cu, ... */
		case TIOCSETAW:		/* cu, ... */
		case TIOCSETAF:		/* tcsetattr TCSAFLUSH, script */
		case TIOCSCTTY:		/* forkpty(3), login_tty(3), ... */
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			break;
		}
	}

	if ((p->p_p->ps_pledge & PLEDGE_ROUTE)) {
		switch (com) {
		case SIOCGIFADDR:
		case SIOCGIFAFLAG_IN6:
		case SIOCGIFALIFETIME_IN6:
		case SIOCGIFDESCR:
		case SIOCGIFFLAGS:
		case SIOCGIFMETRIC:
		case SIOCGIFGMEMB:
		case SIOCGIFRDOMAIN:
		case SIOCGIFDSTADDR_IN6:
		case SIOCGIFNETMASK_IN6:
		case SIOCGIFXFLAGS:
		case SIOCGNBRINFO_IN6:
		case SIOCGIFINFO_IN6:
		case SIOCGIFMEDIA:
			if (fp->f_type == DTYPE_SOCKET)
				return (0);
			break;
		}
	}

	if ((p->p_p->ps_pledge & PLEDGE_WROUTE)) {
		switch (com) {
		case SIOCAIFADDR_IN6:
			if (fp->f_type == DTYPE_SOCKET)
				return (0);
			break;
		case SIOCSIFMTU:
			if (fp->f_type == DTYPE_SOCKET)
				return (0);
			break;
		}
	}

#if NVMM > 0
	if ((p->p_p->ps_pledge & PLEDGE_VMM)) {
		if ((fp->f_type == DTYPE_VNODE) &&
		    (vp->v_type == VCHR) &&
		    (cdevsw[major(vp->v_rdev)].d_open == vmmopen)) {
			error = pledge_ioctl_vmm(p, com);
			if (error == 0)
				return 0;
		}
	}
#endif

	return pledge_fail(p, error, PLEDGE_TTY);
}

int
pledge_sockopt(struct proc *p, int set, int level, int optname)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	/* Always allow these, which are too common to reject */
	switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_RCVBUF:
		case SO_ERROR:
			return 0;
		}
		break;
	}

	if ((p->p_p->ps_pledge & (PLEDGE_INET|PLEDGE_UNIX|PLEDGE_DNS|PLEDGE_YPACTIVE)) == 0)
		return pledge_fail(p, EPERM, PLEDGE_INET);
	/* In use by some service libraries */
	switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_TIMESTAMP:
			return 0;
		}
		break;
	}

	/* DNS resolver may do these requests */
	if ((p->p_p->ps_pledge & PLEDGE_DNS)) {
		switch (level) {
		case IPPROTO_IPV6:
			switch (optname) {
			case IPV6_RECVPKTINFO:
			case IPV6_USE_MIN_MTU:
				return (0);
			}
		}
	}

	/* YP may do these requests */
	if (p->p_p->ps_pledge & PLEDGE_YPACTIVE) {
		switch (level) {
		case IPPROTO_IP:
			switch (optname) {
			case IP_PORTRANGE:
				return (0);
			}
			break;

		case IPPROTO_IPV6:
			switch (optname) {
			case IPV6_PORTRANGE:
				return (0);
			}
			break;
		}
	}

	if ((p->p_p->ps_pledge & (PLEDGE_INET|PLEDGE_UNIX)) == 0)
		return pledge_fail(p, EPERM, PLEDGE_INET);
	switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_RTABLE:
			return pledge_fail(p, EINVAL, PLEDGE_INET);
		}
		return (0);
	}

	if ((p->p_p->ps_pledge & PLEDGE_INET) == 0)
		return pledge_fail(p, EPERM, PLEDGE_INET);
	switch (level) {
	case IPPROTO_TCP:
		switch (optname) {
		case TCP_NODELAY:
		case TCP_MD5SIG:
		case TCP_SACK_ENABLE:
		case TCP_MAXSEG:
		case TCP_NOPUSH:
			return (0);
		}
		break;
	case IPPROTO_IP:
		switch (optname) {
		case IP_OPTIONS:
			if (!set)
				return (0);
			break;
		case IP_TOS:
		case IP_TTL:
		case IP_MINTTL:
		case IP_IPDEFTTL:
		case IP_PORTRANGE:
		case IP_RECVDSTADDR:
		case IP_RECVDSTPORT:
			return (0);
		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			if (p->p_p->ps_pledge & PLEDGE_MCAST)
				return (0);
			break;
		}
		break;
	case IPPROTO_ICMP:
		break;
	case IPPROTO_IPV6:
		switch (optname) {
		case IPV6_TCLASS:
		case IPV6_UNICAST_HOPS:
		case IPV6_MINHOPCOUNT:
		case IPV6_RECVHOPLIMIT:
		case IPV6_PORTRANGE:
		case IPV6_RECVPKTINFO:
		case IPV6_RECVDSTPORT:
		case IPV6_V6ONLY:
			return (0);
		case IPV6_MULTICAST_IF:
		case IPV6_MULTICAST_HOPS:
		case IPV6_MULTICAST_LOOP:
		case IPV6_JOIN_GROUP:
		case IPV6_LEAVE_GROUP:
			if (p->p_p->ps_pledge & PLEDGE_MCAST)
				return (0);
			break;
		}
		break;
	case IPPROTO_ICMPV6:
		break;
	}
	return pledge_fail(p, EPERM, PLEDGE_INET);
}

int
pledge_socket(struct proc *p, int domain, unsigned int state)
{
	if (! ISSET(p->p_p->ps_flags, PS_PLEDGE))
		return 0;

	if (ISSET(state, SS_DNS)) {
		if (ISSET(p->p_p->ps_pledge, PLEDGE_DNS))
			return 0;
		return pledge_fail(p, EPERM, PLEDGE_DNS);
	}

	switch (domain) {
	case -1:		/* accept on any domain */
		return (0);
	case AF_INET:
	case AF_INET6:
		if (ISSET(p->p_p->ps_pledge, PLEDGE_INET) ||
		    ISSET(p->p_p->ps_pledge, PLEDGE_YPACTIVE))
			return 0;
		return pledge_fail(p, EPERM, PLEDGE_INET);

	case AF_UNIX:
		if (ISSET(p->p_p->ps_pledge, PLEDGE_UNIX))
			return 0;
		return pledge_fail(p, EPERM, PLEDGE_UNIX);
	}

	return pledge_fail(p, EINVAL, PLEDGE_INET);
}

int
pledge_flock(struct proc *p)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_FLOCK))
		return (0);
	return (pledge_fail(p, EPERM, PLEDGE_FLOCK));
}

int
pledge_swapctl(struct proc *p)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);
	return (EPERM);
}

/* bsearch over pledgereq. return flags value if found, 0 else */
uint64_t
pledgereq_flags(const char *req_name)
{
	int base = 0, cmp, i, lim;

	for (lim = nitems(pledgereq); lim != 0; lim >>= 1) {
		i = base + (lim >> 1);
		cmp = strcmp(req_name, pledgereq[i].name);
		if (cmp == 0)
			return (pledgereq[i].flags);
		if (cmp > 0) { /* not found before, move right */
			base = i + 1;
			lim--;
		} /* else move left */
	}
	return (0);
}

int
pledge_fcntl(struct proc *p, int cmd)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);
	if ((p->p_p->ps_pledge & PLEDGE_PROC) == 0 && cmd == F_SETOWN)
		return pledge_fail(p, EPERM, PLEDGE_PROC);
	return (0);
}

int
pledge_kill(struct proc *p, pid_t pid)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return 0;
	if (p->p_p->ps_pledge & PLEDGE_PROC)
		return 0;
	if (pid == 0 || pid == p->p_p->ps_pid)
		return 0;
	return pledge_fail(p, EPERM, PLEDGE_PROC);
}

int
pledge_protexec(struct proc *p, int prot)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return 0;
	/* Before kbind(2) call, ld.so and crt may create EXEC mappings */
	if (p->p_p->ps_kbind_addr == 0 && p->p_p->ps_kbind_cookie == 0)
		return 0;
	if (!(p->p_p->ps_pledge & PLEDGE_PROTEXEC) && (prot & PROT_EXEC))
		return pledge_fail(p, EPERM, PLEDGE_PROTEXEC);
	return 0;
}

int
canonpath(const char *input, char *buf, size_t bufsize)
{
	const char *p;
	char *q;

	/* can't canon relative paths, don't bother */
	if (input[0] != '/') {
		if (strlcpy(buf, input, bufsize) >= bufsize)
			return ENAMETOOLONG;
		return 0;
	}

	p = input;
	q = buf;
	while (*p && (q - buf < bufsize)) {
		if (p[0] == '/' && (p[1] == '/' || p[1] == '\0')) {
			p += 1;

		} else if (p[0] == '/' && p[1] == '.' &&
		    (p[2] == '/' || p[2] == '\0')) {
			p += 2;

		} else if (p[0] == '/' && p[1] == '.' && p[2] == '.' &&
		    (p[3] == '/' || p[3] == '\0')) {
			p += 3;
			if (q != buf)	/* "/../" at start of buf */
				while (*--q != '/')
					continue;

		} else {
			*q++ = *p++;
		}
	}
	if ((*p == '\0') && (q - buf < bufsize)) {
		*q = 0;
		return 0;
	} else
		return ENAMETOOLONG;
}
