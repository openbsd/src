/*	$OpenBSD: kern_pledge.c,v 1.84 2015/10/28 12:17:20 deraadt Exp $	*/

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
#include <sys/types.h>

#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/socketvar.h>
#include <sys/vnode.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/ktrace.h>

#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <sys/mtio.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_var.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <netinet/tcp.h>

#include <sys/conf.h>
#include <sys/specdev.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/systm.h>
#define PLEDGENAMES
#include <sys/pledge.h>

#include "pty.h"

int canonpath(const char *input, char *buf, size_t bufsize);
int substrcmp(const char *p1, size_t s1, const char *p2, size_t s2);

const u_int pledge_syscalls[SYS_MAXSYSCALL] = {
	[SYS_exit] = 0xffffffff,
	[SYS_kbind] = 0xffffffff,
	[SYS___get_tcb] = 0xffffffff,

	[SYS_getuid] = PLEDGE_STDIO,
	[SYS_geteuid] = PLEDGE_STDIO,
	[SYS_getresuid] = PLEDGE_STDIO,
	[SYS_getgid] = PLEDGE_STDIO,
	[SYS_getegid] = PLEDGE_STDIO,
	[SYS_getresgid] = PLEDGE_STDIO,
	[SYS_getgroups] = PLEDGE_STDIO,
	[SYS_getlogin] = PLEDGE_STDIO,
	[SYS_getpgrp] = PLEDGE_STDIO,
	[SYS_getpgid] = PLEDGE_STDIO,
	[SYS_getppid] = PLEDGE_STDIO,
	[SYS_getsid] = PLEDGE_STDIO,
	[SYS_getthrid] = PLEDGE_STDIO,
	[SYS_getrlimit] = PLEDGE_STDIO,
	[SYS_gettimeofday] = PLEDGE_STDIO,
	[SYS_getdtablecount] = PLEDGE_STDIO,
	[SYS_getrusage] = PLEDGE_STDIO,
	[SYS_issetugid] = PLEDGE_STDIO,
	[SYS_clock_getres] = PLEDGE_STDIO,
	[SYS_clock_gettime] = PLEDGE_STDIO,
	[SYS_getpid] = PLEDGE_STDIO,
	[SYS_umask] = PLEDGE_STDIO,
	[SYS_sysctl] = PLEDGE_STDIO,	/* read-only; narrow subset */

	[SYS_setsockopt] = PLEDGE_STDIO,	/* white list */
	[SYS_getsockopt] = PLEDGE_STDIO,

	[SYS_fchdir] = PLEDGE_STDIO,	/* careful of directory fd inside jails */

	/* needed by threaded programs */
	[SYS___tfork] = PLEDGE_PROC,
	[SYS_sched_yield] = PLEDGE_STDIO,
	[SYS___thrsleep] = PLEDGE_STDIO,
	[SYS___thrwakeup] = PLEDGE_STDIO,
	[SYS___threxit] = PLEDGE_STDIO,
	[SYS___thrsigdivert] = PLEDGE_STDIO,

	[SYS_sendsyslog] = PLEDGE_STDIO,
	[SYS_nanosleep] = PLEDGE_STDIO,
	[SYS_sigaltstack] = PLEDGE_STDIO,
	[SYS_sigprocmask] = PLEDGE_STDIO,
	[SYS_sigsuspend] = PLEDGE_STDIO,
	[SYS_sigaction] = PLEDGE_STDIO,
	[SYS_sigreturn] = PLEDGE_STDIO,
	[SYS_sigpending] = PLEDGE_STDIO,
	[SYS_getitimer] = PLEDGE_STDIO,
	[SYS_setitimer] = PLEDGE_STDIO,

	[SYS_pledge] = PLEDGE_STDIO,

	[SYS_wait4] = PLEDGE_STDIO,

	[SYS_adjtime] = PLEDGE_STDIO,	/* read-only, unless "settime" */
	[SYS_adjfreq] = PLEDGE_SETTIME,
	[SYS_settimeofday] = PLEDGE_SETTIME,

	[SYS_poll] = PLEDGE_STDIO,
	[SYS_ppoll] = PLEDGE_STDIO,
	[SYS_kevent] = PLEDGE_STDIO,
	[SYS_kqueue] = PLEDGE_STDIO,
	[SYS_select] = PLEDGE_STDIO,

	[SYS_close] = PLEDGE_STDIO,
	[SYS_dup] = PLEDGE_STDIO,
	[SYS_dup2] = PLEDGE_STDIO,
	[SYS_dup3] = PLEDGE_STDIO,
	[SYS_closefrom] = PLEDGE_STDIO,
	[SYS_shutdown] = PLEDGE_STDIO,
	[SYS_read] = PLEDGE_STDIO,
	[SYS_readv] = PLEDGE_STDIO,
	[SYS_pread] = PLEDGE_STDIO,
	[SYS_preadv] = PLEDGE_STDIO,
	[SYS_write] = PLEDGE_STDIO,
	[SYS_writev] = PLEDGE_STDIO,
	[SYS_pwrite] = PLEDGE_STDIO,
	[SYS_pwritev] = PLEDGE_STDIO,
	[SYS_ftruncate] = PLEDGE_STDIO,
	[SYS_lseek] = PLEDGE_STDIO,
	[SYS_fstat] = PLEDGE_STDIO,

	[SYS_fcntl] = PLEDGE_STDIO,
	[SYS_fsync] = PLEDGE_STDIO,
	[SYS_pipe] = PLEDGE_STDIO,
	[SYS_pipe2] = PLEDGE_STDIO,
	[SYS_socketpair] = PLEDGE_STDIO,
	[SYS_getdents] = PLEDGE_STDIO,

	[SYS_sendto] = PLEDGE_STDIO | PLEDGE_YPACTIVE,
	[SYS_sendmsg] = PLEDGE_STDIO,
	[SYS_recvmsg] = PLEDGE_STDIO,
	[SYS_recvfrom] = PLEDGE_STDIO | PLEDGE_YPACTIVE,

	[SYS_fork] = PLEDGE_PROC,
	[SYS_vfork] = PLEDGE_PROC,
	[SYS_setpgid] = PLEDGE_PROC,
	[SYS_setsid] = PLEDGE_PROC,
	[SYS_kill] = PLEDGE_STDIO | PLEDGE_PROC,

	[SYS_setrlimit] = PLEDGE_PROC | PLEDGE_ID,
	[SYS_getpriority] = PLEDGE_PROC | PLEDGE_ID,

	/* XXX we should limit the power for the "proc"-only case */
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

	[SYS_execve] = PLEDGE_EXEC,

	/* FIONREAD/FIONBIO, plus further checks in pledge_ioctl_check() */
	[SYS_ioctl] = PLEDGE_STDIO | PLEDGE_IOCTL | PLEDGE_TTY,

	[SYS_getentropy] = PLEDGE_STDIO,
	[SYS_madvise] = PLEDGE_STDIO,
	[SYS_minherit] = PLEDGE_STDIO,
	[SYS_mmap] = PLEDGE_STDIO,
	[SYS_mprotect] = PLEDGE_STDIO,
	[SYS_mquery] = PLEDGE_STDIO,
	[SYS_munmap] = PLEDGE_STDIO,

	[SYS_open] = PLEDGE_STDIO,		/* further checks in namei */
	[SYS_stat] = PLEDGE_STDIO,		/* further checks in namei */
	[SYS_access] = PLEDGE_STDIO,		/* further checks in namei */
	[SYS_readlink] = PLEDGE_STDIO,		/* further checks in namei */

	[SYS_chdir] = PLEDGE_RPATH,
	[SYS_chroot] = PLEDGE_ID,
	[SYS_openat] = PLEDGE_RPATH | PLEDGE_WPATH,
	[SYS_fstatat] = PLEDGE_RPATH | PLEDGE_WPATH,
	[SYS_faccessat] = PLEDGE_RPATH | PLEDGE_WPATH,
	[SYS_readlinkat] = PLEDGE_RPATH | PLEDGE_WPATH,
	[SYS_lstat] = PLEDGE_RPATH | PLEDGE_WPATH | PLEDGE_TMPPATH,
	[SYS_rename] = PLEDGE_CPATH,
	[SYS_rmdir] = PLEDGE_CPATH,
	[SYS_renameat] = PLEDGE_CPATH,
	[SYS_link] = PLEDGE_CPATH,
	[SYS_linkat] = PLEDGE_CPATH,
	[SYS_symlink] = PLEDGE_CPATH,
	[SYS_unlink] = PLEDGE_CPATH | PLEDGE_TMPPATH,
	[SYS_unlinkat] = PLEDGE_CPATH,
	[SYS_mkdir] = PLEDGE_CPATH,
	[SYS_mkdirat] = PLEDGE_CPATH,

	/*
	 * Classify as RPATH|WPATH, because of path information leakage.
	 * WPATH due to unknown use of mk*temp(3) on non-/tmp paths..
	 */
	[SYS___getcwd] = PLEDGE_RPATH | PLEDGE_WPATH,

	/* Classify as RPATH, because these leak path information */
	[SYS_getfsstat] = PLEDGE_RPATH,
	[SYS_statfs] = PLEDGE_RPATH,
	[SYS_fstatfs] = PLEDGE_RPATH,

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
	[SYS_chown] = PLEDGE_FATTR,
	[SYS_fchownat] = PLEDGE_FATTR,
	[SYS_lchown] = PLEDGE_FATTR,
	[SYS_fchown] = PLEDGE_FATTR,

	[SYS_socket] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS | PLEDGE_YPACTIVE,
	[SYS_connect] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS | PLEDGE_YPACTIVE,
	[SYS_bind] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS,
	[SYS_getsockname] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS,

	[SYS_listen] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_accept4] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_accept] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_getpeername] = PLEDGE_INET | PLEDGE_UNIX,

	[SYS_flock] = PLEDGE_FLOCK | PLEDGE_YPACTIVE,

	[SYS_swapctl] = PLEDGE_VMINFO,	/* XXX should limit to "get" operations */
};

static const struct {
	char *name;
	int flags;
} pledgereq[] = {
	{ "stdio",		PLEDGE_STDIO },
	{ "rpath",		PLEDGE_RPATH },
	{ "wpath",		PLEDGE_WPATH },
	{ "tmppath",		PLEDGE_TMPPATH },
	{ "inet",		PLEDGE_INET },
	{ "unix",		PLEDGE_UNIX },
	{ "dns",		PLEDGE_DNS },
	{ "getpw",		PLEDGE_GETPW },
	{ "sendfd",		PLEDGE_SENDFD },
	{ "recvfd",		PLEDGE_RECVFD },
	{ "ioctl",		PLEDGE_IOCTL },
	{ "id",			PLEDGE_ID },
	{ "route",		PLEDGE_ROUTE },
	{ "mcast",		PLEDGE_MCAST },
	{ "tty",		PLEDGE_TTY },
	{ "proc",		PLEDGE_PROC },
	{ "exec",		PLEDGE_EXEC },
	{ "cpath",		PLEDGE_CPATH },
	{ "fattr",		PLEDGE_FATTR },
	{ "prot_exec",		PLEDGE_PROTEXEC },
	{ "flock",		PLEDGE_FLOCK },
	{ "ps",			PLEDGE_PS },
	{ "vminfo",		PLEDGE_VMINFO },
	{ "settime",		PLEDGE_SETTIME },
	{ "abort",		0 },	/* XXX reserve for later */
};

int
sys_pledge(struct proc *p, void *v, register_t *retval)
{
	struct sys_pledge_args /* {
		syscallarg(const char *)request;
		syscallarg(const char **)paths;
	} */	*uap = v;
	int flags = 0;
	int error;

	if (SCARG(uap, request)) {
		size_t rbuflen;
		char *rbuf, *rp, *pn;
		int f, i;

		rbuf = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
		error = copyinstr(SCARG(uap, request), rbuf, MAXPATHLEN,
		    &rbuflen);
		if (error) {
			free(rbuf, M_TEMP, MAXPATHLEN);
			return (error);
		}
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrstruct(p, "pledgereq", rbuf, rbuflen-1);
#endif

		for (rp = rbuf; rp && *rp && error == 0; rp = pn) {
			pn = strchr(rp, ' ');	/* find terminator */
			if (pn) {
				while (*pn == ' ')
					*pn++ = '\0';
			}

			for (f = i = 0; i < nitems(pledgereq); i++) {
				if (strcmp(rp, pledgereq[i].name) == 0) {
					f = pledgereq[i].flags;
					break;
				}
			}
			if (f == 0) {
				free(rbuf, M_TEMP, MAXPATHLEN);
				return (EINVAL);
			}
			flags |= f;
		}
		free(rbuf, M_TEMP, MAXPATHLEN);
	}

	if (flags & ~PLEDGE_USERSET)
		return (EINVAL);

	if ((p->p_p->ps_flags & PS_PLEDGE)) {
		/* Already pledged, only allow reductions */
		if (((flags | p->p_p->ps_pledge) & PLEDGE_USERSET) !=
		    (p->p_p->ps_pledge & PLEDGE_USERSET)) {
			return (EPERM);
		}

		flags &= p->p_p->ps_pledge;
		flags &= PLEDGE_USERSET;		/* Relearn _ACTIVE */
	}

	if (SCARG(uap, paths)) {
		const char **u = SCARG(uap, paths), *sp;
		struct whitepaths *wl;
		char *cwdpath = NULL, *path;
		size_t cwdpathlen = MAXPATHLEN * 4, cwdlen, len, maxargs = 0;
		int i, error;

		if (p->p_p->ps_pledgepaths)
			return (EPERM);

		/* Count paths */
		for (i = 0; i < PLEDGE_MAXPATHS; i++) {
			if ((error = copyin(u + i, &sp, sizeof(sp))) != 0)
				return (error);
			if (sp == NULL)
				break;
		}
		if (i == PLEDGE_MAXPATHS)
			return (E2BIG);

		wl = malloc(sizeof *wl + sizeof(struct whitepath) * (i+1),
		    M_TEMP, M_WAITOK | M_ZERO);
		wl->wl_size = sizeof *wl + sizeof(struct whitepath) * (i+1);
		wl->wl_count = i;
		wl->wl_ref = 1;

		path = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);

		/* Copy in */
		for (i = 0; i < wl->wl_count; i++) {
			char *fullpath = NULL, *builtpath = NULL, *canopath = NULL, *cwd;
			size_t builtlen = 0;

			if ((error = copyin(u + i, &sp, sizeof(sp))) != 0)
				break;
			if (sp == NULL)
				break;
			if ((error = copyinstr(sp, path, MAXPATHLEN, &len)) != 0)
				break;
#ifdef KTRACE
			if (KTRPOINT(p, KTR_STRUCT))
				ktrstruct(p, "pledgepath", path, len-1);
#endif

			/* If path is relative, prepend cwd */
			if (path[0] != '/') {
				if (cwdpath == NULL) {
					char *bp, *bpend;

					cwdpath = malloc(cwdpathlen, M_TEMP, M_WAITOK);
					bp = &cwdpath[cwdpathlen];
					bpend = bp;
					*(--bp) = '\0';

					error = vfs_getcwd_common(p->p_fd->fd_cdir,
					    NULL, &bp, cwdpath, cwdpathlen/2,
					    GETCWD_CHECK_ACCESS, p);
					if (error)
						break;
					cwd = bp;
					cwdlen = (bpend - bp);
				}

				/* NUL included in cwd component */
				builtlen = cwdlen + 1 + strlen(path);
				if (builtlen > PATH_MAX) {
					error = ENAMETOOLONG;
					break;
				}
				builtpath = malloc(builtlen, M_TEMP, M_WAITOK);
				snprintf(builtpath, builtlen, "%s/%s", cwd, path);
				// printf("pledge: builtpath = %s\n", builtpath);
				fullpath = builtpath;
			} else
				fullpath = path;

			canopath = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
			error = canonpath(fullpath, canopath, MAXPATHLEN);

			free(builtpath, M_TEMP, builtlen);
			if (error != 0) {
				free(canopath, M_TEMP, MAXPATHLEN);
				break;
			}

			len = strlen(canopath) + 1;

			//printf("pledge: canopath = %s %lld strlen %lld\n", canopath,
			//    (long long)len, (long long)strlen(canopath));

			if (maxargs += len > ARG_MAX) {
				error = E2BIG;
				break;
			}
			wl->wl_paths[i].name = malloc(len, M_TEMP, M_WAITOK);
			memcpy(wl->wl_paths[i].name, canopath, len);
			wl->wl_paths[i].len = len;
			free(canopath, M_TEMP, MAXPATHLEN);
		}
		free(path, M_TEMP, MAXPATHLEN);
		free(cwdpath, M_TEMP, cwdpathlen);

		if (error) {
			for (i = 0; i < wl->wl_count; i++)
				free(wl->wl_paths[i].name,
				    M_TEMP, wl->wl_paths[i].len);
			free(wl, M_TEMP, wl->wl_size);
			return (error);
		}
		p->p_p->ps_pledgepaths = wl;
#if 0
		printf("pledge: %s(%d): paths loaded:\n", p->p_comm, p->p_pid);
		for (i = 0; i < wl->wl_count; i++)
			if (wl->wl_paths[i].name)
				printf("pledge: %d=%s %lld\n", i, wl->wl_paths[i].name,
				    (long long)wl->wl_paths[i].len);
#endif
	}

	p->p_p->ps_pledge = flags;
	p->p_p->ps_pledge |= PLEDGE_COREDUMP;	/* XXX temporary */
	p->p_p->ps_flags |= PS_PLEDGE;
	return (0);
}

int
pledge_check(struct proc *p, int code, int *tval)
{
	p->p_pledgenote = p->p_pledgeafter = 0;	/* XX optimise? */
	p->p_pledge_syscall = code;
	*tval = 0;

	if (code < 0 || code > SYS_MAXSYSCALL - 1)
		return (EINVAL);

	if ((p->p_p->ps_pledge == 0) &&
	    (code == SYS_exit || code == SYS_kbind))
		return (0);

	if (p->p_p->ps_pledge & pledge_syscalls[code])
		return (0);

	*tval = pledge_syscalls[code];
	return (EPERM);
}

int
pledge_fail(struct proc *p, int error, int code)
{
	char *codes = "";
	int i;

	/* Print first matching pledge */
	for (i = 0; code && pledgenames[i].bits != 0; i++)
		if (pledgenames[i].bits & code) {
			codes = pledgenames[i].name;
			break;
		}
	printf("%s(%d): syscall %d \"%s\"\n", p->p_comm, p->p_pid,
	    p->p_pledge_syscall, codes);
#ifdef KTRACE
	ktrpledge(p, error, code, p->p_pledge_syscall);
#endif
	if (p->p_p->ps_pledge & PLEDGE_COREDUMP) {
		/* Core dump requested */
		struct sigaction sa;

		memset(&sa, 0, sizeof sa);
		sa.sa_handler = SIG_DFL;
		setsigvec(p, SIGABRT, &sa);
		psignal(p, SIGABRT);
	} else
		psignal(p, SIGKILL);

	p->p_p->ps_pledge = 0;		/* Disable all PLEDGE_ flags */
	return (error);
}

/*
 * Need to make it more obvious that one cannot get through here
 * without the right flags set
 */
int
pledge_namei(struct proc *p, char *origpath)
{
	char path[PATH_MAX];
	int error;

	if (p->p_pledgenote == PLEDGE_COREDUMP)
		return (0);			/* Allow a coredump */

	error = canonpath(origpath, path, sizeof(path));
	if (error)
		return (pledge_fail(p, error, p->p_pledgenote));

	/* chmod(2), chflags(2), ... */
	if ((p->p_pledgenote & PLEDGE_FATTR) &&
	    (p->p_p->ps_pledge & PLEDGE_FATTR) == 0) {
		return (pledge_fail(p, EPERM, PLEDGE_FATTR));
	}

	/* Detect what looks like a mkstemp(3) family operation */
	if ((p->p_p->ps_pledge & PLEDGE_TMPPATH) &&
	    (p->p_pledge_syscall == SYS_open) &&
	    (p->p_pledgenote & PLEDGE_CPATH) &&
	    strncmp(path, "/tmp/", sizeof("/tmp/") - 1) == 0) {
		return (0);
	}

	/* Allow unlinking of a mkstemp(3) file...
	 * Good opportunity for strict checks here.
	 */
	if ((p->p_p->ps_pledge & PLEDGE_TMPPATH) &&
	    (p->p_pledge_syscall == SYS_unlink) &&
	    strncmp(path, "/tmp/", sizeof("/tmp/") - 1) == 0) {
		return (0);
	}

	/* open, mkdir, or other path creation operation */
	if ((p->p_pledgenote & PLEDGE_CPATH) &&
	    ((p->p_p->ps_pledge & PLEDGE_CPATH) == 0))
		return (pledge_fail(p, EPERM, PLEDGE_CPATH));

	/* Doing a permitted execve() */
	if ((p->p_pledgenote & PLEDGE_EXEC) &&
	    (p->p_p->ps_pledge & PLEDGE_EXEC))
		return (0);

	/* Whitelisted read/write paths */
	switch (p->p_pledge_syscall) {
	case SYS_open:
		/* daemon(3) or other such functions */
		if ((p->p_pledgenote & ~(PLEDGE_RPATH | PLEDGE_WPATH)) == 0 &&
		    strcmp(path, "/dev/null") == 0) {
			return (0);
		}

		/* readpassphrase(3), getpw*(3) */
		if ((p->p_p->ps_pledge & (PLEDGE_TTY | PLEDGE_GETPW)) &&
		    (p->p_pledgenote & ~(PLEDGE_RPATH | PLEDGE_WPATH)) == 0 &&
		    strcmp(path, "/dev/tty") == 0) {
			return (0);
		}
		break;
	}

	/* ensure PLEDGE_WPATH request for doing write */
	if ((p->p_pledgenote & PLEDGE_WPATH) &&
	    (p->p_p->ps_pledge & PLEDGE_WPATH) == 0)
		return (pledge_fail(p, EPERM, PLEDGE_WPATH));

	/* Whitelisted read-only paths */
	switch (p->p_pledge_syscall) {
	case SYS_access:
		/* tzset() needs this. */
		if ((p->p_pledgenote == PLEDGE_RPATH) &&
		    strcmp(path, "/etc/localtime") == 0)
			return (0);
		break;
	case SYS_open:
		/* getpw* and friends need a few files */
		if ((p->p_pledgenote == PLEDGE_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_GETPW)) {
			if (strcmp(path, "/etc/spwd.db") == 0)
				return (EPERM);
			if (strcmp(path, "/etc/pwd.db") == 0)
				return (0);
			if (strcmp(path, "/etc/group") == 0)
				return (0);
		}

		/* DNS needs /etc/{resolv.conf,hosts,services}. */
		if ((p->p_pledgenote == PLEDGE_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_DNS)) {
			if (strcmp(path, "/etc/resolv.conf") == 0)
				return (0);
			if (strcmp(path, "/etc/hosts") == 0)
				return (0);
			if (strcmp(path, "/etc/services") == 0)
				return (0);
		}

		if ((p->p_pledgenote == PLEDGE_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_GETPW)) {
			if (strcmp(path, "/var/run/ypbind.lock") == 0) {
				p->p_pledgeafter |= PLEDGE_YPACTIVE;
				return (0);
			}
			if (strncmp(path, "/var/yp/binding/",
			    sizeof("/var/yp/binding/") - 1) == 0)
				return (0);
		}

		/* tzset() needs these. */
		if ((p->p_pledgenote == PLEDGE_RPATH) &&
		    strncmp(path, "/usr/share/zoneinfo/",
		    sizeof("/usr/share/zoneinfo/") - 1) == 0)
			return (0);
		if ((p->p_pledgenote == PLEDGE_RPATH) &&
		    strcmp(path, "/etc/localtime") == 0)
			return (0);

		/* /usr/share/nls/../libc.cat has to succeed for strerror(3). */
		if ((p->p_pledgenote == PLEDGE_RPATH) &&
		    strncmp(path, "/usr/share/nls/",
		    sizeof("/usr/share/nls/") - 1) == 0 &&
		    strcmp(path + strlen(path) - 9, "/libc.cat") == 0)
			return (0);

		break;
	case SYS_readlink:
		/* Allow /etc/malloc.conf for malloc(3). */
		if ((p->p_pledgenote == PLEDGE_RPATH) &&
		    strcmp(path, "/etc/malloc.conf") == 0)
			return (0);
		break;
	case SYS_stat:
		/* DNS needs /etc/resolv.conf. */
		if ((p->p_pledgenote == PLEDGE_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_DNS) &&
		    strcmp(path, "/etc/resolv.conf") == 0)
			return (0);
		break;
	case SYS_chroot:
		/* Allowed for "proc id" */
		if ((p->p_p->ps_pledge & PLEDGE_PROC))
			return (0);
		break;
	}

	/* ensure PLEDGE_RPATH request for doing read */
	if ((p->p_pledgenote & PLEDGE_RPATH) &&
	    (p->p_p->ps_pledge & PLEDGE_RPATH) == 0)
		return (pledge_fail(p, EPERM, PLEDGE_RPATH));

	/*
	 * If a whitelist is set, compare canonical paths.  Anything
	 * not on the whitelist gets ENOENT.
	 */
	if (p->p_p->ps_pledgepaths) {
		struct whitepaths *wl = p->p_p->ps_pledgepaths;
		char *fullpath, *builtpath = NULL, *canopath = NULL;
		size_t builtlen = 0, canopathlen;
		int i, error, pardir_found;

		if (origpath[0] != '/') {
			char *cwdpath, *cwd, *bp, *bpend;
			size_t cwdpathlen = MAXPATHLEN * 4, cwdlen;

			cwdpath = malloc(cwdpathlen, M_TEMP, M_WAITOK);
			bp = &cwdpath[cwdpathlen];
			bpend = bp;
			*(--bp) = '\0';

			error = vfs_getcwd_common(p->p_fd->fd_cdir,
			    NULL, &bp, cwdpath, cwdpathlen/2,
			    GETCWD_CHECK_ACCESS, p);
			if (error) {
				free(cwdpath, M_TEMP, cwdpathlen);
				return (error);
			}
			cwd = bp;
			cwdlen = (bpend - bp);

			/* NUL included in cwd component */
			builtlen = cwdlen + 1 + strlen(origpath);
			builtpath = malloc(builtlen, M_TEMP, M_WAITOK);
			snprintf(builtpath, builtlen, "%s/%s", cwd, origpath);
			fullpath = builtpath;
			free(cwdpath, M_TEMP, cwdpathlen);

			//printf("namei: builtpath = %s %lld strlen %lld\n", builtpath,
			//    (long long)builtlen, (long long)strlen(builtpath));
		} else
			fullpath = path;

		canopath = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
		error = canonpath(fullpath, canopath, MAXPATHLEN);

		free(builtpath, M_TEMP, builtlen);
		if (error != 0) {
			free(canopath, M_TEMP, MAXPATHLEN);
			return (pledge_fail(p, error, p->p_pledgenote));
		}

		//printf("namei: canopath = %s strlen %lld\n", canopath,
		//    (long long)strlen(canopath));

		error = ENOENT;
		canopathlen = strlen(canopath);
		pardir_found = 0;
		for (i = 0; i < wl->wl_count && wl->wl_paths[i].name && error; i++) {
			int substr = substrcmp(wl->wl_paths[i].name,
			    wl->wl_paths[i].len - 1, canopath, canopathlen);

			//printf("pledge: check: %s [%ld] %s [%ld] = %d\n",
			//    wl->wl_paths[i].name, wl->wl_paths[i].len - 1,
			//    canopath, canopathlen,
			//    substr);

			/* wl_paths[i].name is a substring of canopath */
			if (substr == 1) {
				u_char term = canopath[wl->wl_paths[i].len - 1];

				if (term == '\0' || term == '/' ||
				    wl->wl_paths[i].name[1] == '\0')
					error = 0;

			/* canopath is a substring of wl_paths[i].name */
			} else if (substr == 2) {
				u_char term = wl->wl_paths[i].name[canopathlen];

				if (canopath[1] == '\0' || term == '/')
					pardir_found = 1;
			}
		}
		if (pardir_found)
			switch (p->p_pledge_syscall) {
			case SYS_stat:
			case SYS_lstat:
			case SYS_fstatat:
			case SYS_fstat:
				p->p_pledgenote |= PLEDGE_STATLIE;
				error = 0;
			}
		free(canopath, M_TEMP, MAXPATHLEN);
		return (error);			/* Don't hint why it failed */
	}

	if (p->p_p->ps_pledge & PLEDGE_RPATH)
		return (0);
	if (p->p_p->ps_pledge & PLEDGE_WPATH)
		return (0);
	if (p->p_p->ps_pledge & PLEDGE_CPATH)
		return (0);

	return (pledge_fail(p, EPERM, p->p_pledgenote));
}

void
pledge_aftersyscall(struct proc *p, int code, int error)
{
	if ((p->p_pledgeafter & PLEDGE_YPACTIVE) && error == 0)
		atomic_setbits_int(&p->p_p->ps_pledge, PLEDGE_YPACTIVE | PLEDGE_INET);
}

/*
 * Only allow reception of safe file descriptors.
 */
int
pledge_recvfd_check(struct proc *p, struct file *fp)
{
	struct vnode *vp = NULL;
	char *vtypes[] = { VTYPE_NAMES };

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);
	if ((p->p_p->ps_pledge & PLEDGE_RECVFD) == 0) {
		printf("recvmsg not allowed\n");
		return pledge_fail(p, EPERM, PLEDGE_RECVFD);
	}

	switch (fp->f_type) {
	case DTYPE_SOCKET:
	case DTYPE_PIPE:
		return (0);
	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;

		if (vp->v_type != VDIR)
			return (0);
		break;
	}
	printf("recvfd type %d %s\n", fp->f_type, vp ? vtypes[vp->v_type] : "");
	return pledge_fail(p, EINVAL, PLEDGE_RECVFD);
}

/*
 * Only allow sending of safe file descriptors.
 */
int
pledge_sendfd_check(struct proc *p, struct file *fp)
{
	struct vnode *vp = NULL;
	char *vtypes[] = { VTYPE_NAMES };

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_SENDFD) == 0) {
		printf("sendmsg not allowed\n");
		return pledge_fail(p, EPERM, PLEDGE_SENDFD);
	}

	switch (fp->f_type) {
	case DTYPE_SOCKET:
	case DTYPE_PIPE:
		return (0);
	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;

		if (vp->v_type != VDIR)
			return (0);
		break;
	}
	printf("sendfd type %d %s\n", fp->f_type, vp ? vtypes[vp->v_type] : "");
	return pledge_fail(p, EINVAL, PLEDGE_SENDFD);
}

int
pledge_sysctl_check(struct proc *p, int miblen, int *mib, void *new)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if (new)
		return (EFAULT);

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

		if (miblen == 7 &&			/* exposes MACs */
		    mib[0] == CTL_NET && mib[1] == PF_ROUTE &&
		    mib[2] == 0 &&
		    (mib[3] == 0 || mib[3] == AF_INET6 || mib[3] == AF_INET) &&
		    mib[4] == NET_RT_FLAGS && mib[5] == RTF_LLINFO)
			return (0);
	}

	if (p->p_p->ps_pledge & (PLEDGE_PS | PLEDGE_VMINFO)) {
		if (miblen == 2 &&			/* kern.fscale */
		    mib[0] == CTL_KERN && mib[1] == KERN_FSCALE)
			return (0);
		if (miblen == 2 &&			/* kern.boottime */
		    mib[0] == CTL_KERN && mib[1] == KERN_BOOTTIME)
			return (0);
		if (miblen == 2 &&			/* kern.consdev */
		    mib[0] == CTL_KERN && mib[1] == KERN_CONSDEV)
			return (0);
		if (miblen == 2 &&			/* kern.loadavg */
		    mib[0] == CTL_VM && mib[1] == VM_LOADAVG)
			return (0);
		if (miblen == 2 &&			/* kern.cptime */
		    mib[0] == CTL_KERN && mib[1] == KERN_CPTIME)
			return (0);
		if (miblen == 3 &&			/* kern.cptime2 */
		    mib[0] == CTL_KERN && mib[1] == KERN_CPTIME2)
			return (0);
	}

	if ((p->p_p->ps_pledge & PLEDGE_PS)) {
		if (miblen == 4 &&			/* kern.procargs.* */
		    mib[0] == CTL_KERN && mib[1] == KERN_PROC_ARGS &&
		    (mib[3] == KERN_PROC_ARGV || mib[3] == KERN_PROC_ENV))
			return (0);
		if (miblen == 6 &&			/* kern.proc.* */
		    mib[0] == CTL_KERN && mib[1] == KERN_PROC)
			return (0);
		if (miblen == 3 &&			/* kern.proc_cwd.* */
		    mib[0] == CTL_KERN && mib[1] == KERN_PROC_CWD)
			return (0);
		if (miblen == 2 &&			/* hw.physmem */
		    mib[0] == CTL_HW && mib[1] == HW_PHYSMEM64)
			return (0);
		if (miblen == 2 &&			/* kern.ccpu */
		    mib[0] == CTL_KERN && mib[1] == KERN_CCPU)
			return (0);
		if (miblen == 2 &&			/* vm.maxslp */
		    mib[0] == CTL_VM && mib[1] == VM_MAXSLP)
			return (0);
	}

	if ((p->p_p->ps_pledge & PLEDGE_VMINFO)) {
		if (miblen == 2 &&			/* vm.uvmexp */
		    mib[0] == CTL_VM && mib[1] == VM_UVMEXP)
			return (0);
		if (miblen == 3 &&			/* vfs.generic.bcachestat */
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

	/* used by ntpd(8) to read sensors. */
	if (miblen >= 3 &&
	    mib[0] == CTL_HW && mib[1] == HW_SENSORS)
		return (0);

	if (miblen == 2 &&			/* getdomainname() */
	    mib[0] == CTL_KERN && mib[1] == KERN_DOMAINNAME)
		return (0);
	if (miblen == 2 &&			/* gethostname() */
	    mib[0] == CTL_KERN && mib[1] == KERN_HOSTNAME)
		return (0);
	if (miblen == 6 &&		/* if_nameindex() */
	    mib[0] == CTL_NET && mib[1] == PF_ROUTE &&
	    mib[2] == 0 && mib[3] == 0 && mib[4] == NET_RT_IFNAMES)
		return (0);
	if (miblen == 2 &&			/* uname() */
	    mib[0] == CTL_KERN && mib[1] == KERN_OSTYPE)
		return (0);
	if (miblen == 2 &&			/* uname() */
	    mib[0] == CTL_KERN && mib[1] == KERN_OSRELEASE)
		return (0);
	if (miblen == 2 &&			/* uname() */
	    mib[0] == CTL_KERN && mib[1] == KERN_OSVERSION)
		return (0);
	if (miblen == 2 &&			/* uname() */
	    mib[0] == CTL_KERN && mib[1] == KERN_VERSION)
		return (0);
	if (miblen == 2 &&			/* kern.clockrate */
	    mib[0] == CTL_KERN && mib[1] == KERN_CLOCKRATE)
		return (0);
	if (miblen == 2 &&			/* uname() */
	    mib[0] == CTL_HW && mib[1] == HW_MACHINE)
		return (0);
	if (miblen == 2 &&			/* getpagesize() */
	    mib[0] == CTL_HW && mib[1] == HW_PAGESIZE)
		return (0);
	if (miblen == 2 &&			/* setproctitle() */
	    mib[0] == CTL_VM && mib[1] == VM_PSSTRINGS)
		return (0);
	if (miblen == 2 &&			/* hw.ncpu */
	    mib[0] == CTL_HW && mib[1] == HW_NCPU)
		return (0);

	printf("%s(%d): sysctl %d: %d %d %d %d %d %d\n",
	    p->p_comm, p->p_pid, miblen, mib[0], mib[1],
	    mib[2], mib[3], mib[4], mib[5]);
	return (EPERM);
}

int
pledge_chown_check(struct proc *p, uid_t uid, gid_t gid)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if (uid != -1 && uid != p->p_ucred->cr_uid)
		return (EPERM);
	if (gid != -1 && !groupmember(gid, p->p_ucred))
		return (EPERM);
	return (0);
}

int
pledge_adjtime_check(struct proc *p, const void *v)
{
	const struct timeval *delta = v;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_SETTIME))
		return (0);
	if (delta)
		return (EFAULT);
	return (0);
}

int
pledge_sendit_check(struct proc *p, const void *to)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & (PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS)))
		return (0);		/* may use address */
	if (to == NULL)
		return (0);		/* behaves just like write */
	return (EPERM);
}

int
pledge_ioctl_check(struct proc *p, long com, void *v)
{
	struct file *fp = v;
	struct vnode *vp = NULL;

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
	vp = (struct vnode *)fp->f_data;

	/*
	 * Further sets of ioctl become available, but are checked a
	 * bit more carefully against the vnode.
	 */
	if ((p->p_p->ps_pledge & PLEDGE_IOCTL)) {
		switch (com) {
		case TIOCGETA:
		case TIOCGPGRP:
		case TIOCGWINSZ:	/* ENOTTY return for non-tty */
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			return (ENOTTY);
		case BIOCGSTATS:	/* bpf: tcpdump privsep on ^C */
			if (fp->f_type == DTYPE_VNODE &&
			    fp->f_ops->fo_ioctl == vn_ioctl)
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
			if ((p->p_p->ps_pledge & PLEDGE_INET) &&
			    fp->f_type == DTYPE_SOCKET)
				return (0);
			break;
		}
	}

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
#endif /* NPTY > 0 */
		case TIOCSTI:		/* ksh? csh? */
			if ((p->p_p->ps_pledge & PLEDGE_PROC) &&
			    fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			break;
		case TIOCSPGRP:
			if ((p->p_p->ps_pledge & PLEDGE_PROC) == 0)
				break;
			/* FALLTHROUGH */
		case TIOCGPGRP:
		case TIOCGETA:
		case TIOCGWINSZ:	/* ENOTTY return for non-tty */
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			return (ENOTTY);
		case TIOCSWINSZ:
		case TIOCCBRK:		/* cu */
		case TIOCSBRK:		/* cu */
		case TIOCCDTR:		/* cu */
		case TIOCSDTR:		/* cu */
		case TIOCEXCL:		/* cu */
		case TIOCSETA:		/* cu, ... */
		case TIOCSETAW:		/* cu, ... */
		case TIOCSETAF:		/* tcsetattr TCSAFLUSH, script */
		case TIOCFLUSH:		/* getty */
		case TIOCSCTTY:		/* forkpty(3), login_tty(3), ... */
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			break;
		}
	}

	if ((p->p_p->ps_pledge & PLEDGE_ROUTE)) {
		switch (com) {
		case SIOCGIFADDR:
		case SIOCGIFFLAGS:
		case SIOCGIFMETRIC:
		case SIOCGIFGMEMB:
		case SIOCGIFRDOMAIN:
		case SIOCGIFDSTADDR_IN6:
		case SIOCGIFNETMASK_IN6:
		case SIOCGNBRINFO_IN6:
		case SIOCGIFINFO_IN6:
			if (fp->f_type == DTYPE_SOCKET)
				return (0);
			break;
		}
	}

	return pledge_fail(p, EPERM, PLEDGE_IOCTL);
}

int
pledge_sockopt_check(struct proc *p, int set, int level, int optname)
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

	if ((p->p_p->ps_pledge & (PLEDGE_INET|PLEDGE_UNIX|PLEDGE_DNS)) == 0)
	 	return (EINVAL);
	/* In use by some service libraries */
	switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_TIMESTAMP:
			return 0;
		}
		break;
	}

	if ((p->p_p->ps_pledge & (PLEDGE_INET|PLEDGE_UNIX)) == 0)
		return (EINVAL);
	switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_RTABLE:
			return (EINVAL);
		}
		return (0);
	}

	if ((p->p_p->ps_pledge & PLEDGE_INET) == 0)
		return (EINVAL);
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
		case IP_PORTRANGE:
		case IP_RECVDSTADDR:
		case IP_RECVDSTPORT:
			return (0);
		case IP_MULTICAST_IF:
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
		case IPV6_UNICAST_HOPS:
		case IPV6_RECVHOPLIMIT:
		case IPV6_PORTRANGE:
		case IPV6_RECVPKTINFO:
		case IPV6_RECVDSTPORT:
#ifdef notyet
		case IPV6_V6ONLY:
#endif
			return (0);
		case IPV6_MULTICAST_IF:
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
	return (EPERM);
}

int
pledge_socket_check(struct proc *p, int dns)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if (dns && (p->p_p->ps_pledge & PLEDGE_DNS))
		return (0);
	if ((p->p_p->ps_pledge & (PLEDGE_INET|PLEDGE_UNIX|PLEDGE_YPACTIVE)))
		return (0);
	return (EPERM);
}

int
pledge_flock_check(struct proc *p)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_FLOCK))
		return (0);
	return (pledge_fail(p, EPERM, PLEDGE_FLOCK));
}

int
pledge_swapctl_check(struct proc *p)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);
	return (EPERM);
}

int
pledge_fcntl_check(struct proc *p, int cmd)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);
	if ((p->p_p->ps_pledge & PLEDGE_PROC) == 0 &&
	    cmd == F_SETOWN)
		return (EPERM);
	return (0);
}

void
pledge_dropwpaths(struct process *pr)
{
	if (pr->ps_pledgepaths && --pr->ps_pledgepaths->wl_ref == 0) {
		struct whitepaths *wl = pr->ps_pledgepaths;
		int i;

		for (i = 0; i < wl->wl_count; i++)
			free(wl->wl_paths[i].name, M_TEMP, wl->wl_paths[i].len);
		free(wl, M_TEMP, wl->wl_size);
	}
	pr->ps_pledgepaths = NULL;
}

int
canonpath(const char *input, char *buf, size_t bufsize)
{
	char *p, *q, *s, *end;

	/* can't canon relative paths, don't bother */
	if (input[0] != '/') {
		if (strlcpy(buf, input, bufsize) >= bufsize)
			return (ENAMETOOLONG);
		return (0);
	}

	/* easiest to work with strings always ending in '/' */
	if (snprintf(buf, bufsize, "%s/", input) >= bufsize)
		return (ENAMETOOLONG);

	/* after this we will only be shortening the string. */
	p = buf;
	q = p;
	while (*p) {
		if (p[0] == '/' && p[1] == '/') {
			p += 1;
		} else if (p[0] == '/' && p[1] == '.' &&
		    p[2] == '/') {
			p += 2;
		} else {
			*q++ = *p++;
		}
	}
	*q = 0;

	end = buf + strlen(buf);
	s = buf;
	p = s;
	while (1) {
		/* find "/../" (where's strstr when you need it?) */
		while (p < end) {
			if (p[0] == '/' && strncmp(p + 1, "../", 3) == 0)
				break;
			p++;
		}
		if (p == end)
			break;
		if (p == s) {
			memmove(s, p + 3, end - p - 3 + 1);
			end -= 3;
		} else {
			/* s starts with '/', so we know there's one
			 * somewhere before p. */
			q = p - 1;
			while (*q != '/')
				q--;
			memmove(q, p + 3, end - p - 3 + 1);
			end -= p + 3 - q;
			p = q;
		}
	}
	if (end > s + 1)
		*(end - 1) = 0; /* remove trailing '/' */

	return 0;
}

int
substrcmp(const char *p1, size_t s1, const char *p2, size_t s2)
{
	size_t i;
	for (i = 0; i < s1 || i < s2; i++) {
		if (p1[i] != p2[i])
			break;
	}
	if (i == s1) {
		return (1);	/* string1 is a subpath of string2 */
	} else if (i == s2)
		return (2);	/* string2 is a subpath of string1 */
	else
		return (0);	/* no subpath */
}
