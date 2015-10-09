/*	$OpenBSD: kern_pledge.c,v 1.4 2015/10/09 05:30:03 deraadt Exp $	*/

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

#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/systm.h>
#include <sys/pledge.h>

int canonpath(const char *input, char *buf, size_t bufsize);

const u_int pledge_syscalls[SYS_MAXSYSCALL] = {
	[SYS_exit] = 0xffffffff,
	[SYS_kbind] = 0xffffffff,

	[SYS_getuid] = PLEDGE_SELF,
	[SYS_geteuid] = PLEDGE_SELF,
	[SYS_getresuid] = PLEDGE_SELF,
	[SYS_getgid] = PLEDGE_SELF,
	[SYS_getegid] = PLEDGE_SELF,
	[SYS_getresgid] = PLEDGE_SELF,
	[SYS_getgroups] = PLEDGE_SELF,
	[SYS_getlogin] = PLEDGE_SELF,
	[SYS_getpgrp] = PLEDGE_SELF,
	[SYS_getpgid] = PLEDGE_SELF,
	[SYS_getppid] = PLEDGE_SELF,
	[SYS_getsid] = PLEDGE_SELF,
	[SYS_getthrid] = PLEDGE_SELF,
	[SYS_getrlimit] = PLEDGE_SELF,
	[SYS_gettimeofday] = PLEDGE_SELF,
	[SYS_getdtablecount] = PLEDGE_SELF,
	[SYS_getrusage] = PLEDGE_SELF,
	[SYS_issetugid] = PLEDGE_SELF,
	[SYS_clock_getres] = PLEDGE_SELF,
	[SYS_clock_gettime] = PLEDGE_SELF,
	[SYS_getpid] = PLEDGE_SELF,
	[SYS_umask] = PLEDGE_SELF,
	[SYS_sysctl] = PLEDGE_SELF,	/* read-only; narrow subset */
	[SYS_adjtime] = PLEDGE_SELF,	/* read-only */

	[SYS_fchdir] = PLEDGE_SELF,	/* careful of directory fd inside jails */

	/* needed by threaded programs */
	[SYS_sched_yield] = PLEDGE_SELF,
	[SYS___thrsleep] = PLEDGE_SELF,
	[SYS___thrwakeup] = PLEDGE_SELF,
	[SYS___threxit] = PLEDGE_SELF,
	[SYS___thrsigdivert] = PLEDGE_SELF,

	[SYS_sendsyslog] = PLEDGE_SELF,
	[SYS_nanosleep] = PLEDGE_SELF,
	[SYS_sigprocmask] = PLEDGE_SELF,
	[SYS_sigaction] = PLEDGE_SELF,
	[SYS_sigreturn] = PLEDGE_SELF,
	[SYS_sigpending] = PLEDGE_SELF,
	[SYS_getitimer] = PLEDGE_SELF,
	[SYS_setitimer] = PLEDGE_SELF,

	[SYS_pledge] = PLEDGE_SELF,

	[SYS_wait4] = PLEDGE_SELF,

	[SYS_poll] = PLEDGE_RW,
	[SYS_kevent] = PLEDGE_RW,
	[SYS_kqueue] = PLEDGE_RW,
	[SYS_select] = PLEDGE_RW,

	[SYS_close] = PLEDGE_RW,
	[SYS_dup] = PLEDGE_RW,
	[SYS_dup2] = PLEDGE_RW,
	[SYS_dup3] = PLEDGE_RW,
	[SYS_closefrom] = PLEDGE_RW,
	[SYS_shutdown] = PLEDGE_RW,
	[SYS_read] = PLEDGE_RW,
	[SYS_readv] = PLEDGE_RW,
	[SYS_pread] = PLEDGE_RW,
	[SYS_preadv] = PLEDGE_RW,
	[SYS_write] = PLEDGE_RW,
	[SYS_writev] = PLEDGE_RW,
	[SYS_pwrite] = PLEDGE_RW,
	[SYS_pwritev] = PLEDGE_RW,
	[SYS_ftruncate] = PLEDGE_RW,
	[SYS_lseek] = PLEDGE_RW,
	[SYS_fstat] = PLEDGE_RW,

	[SYS_fcntl] = PLEDGE_RW,
	[SYS_fsync] = PLEDGE_RW,
	[SYS_pipe] = PLEDGE_RW,
	[SYS_pipe2] = PLEDGE_RW,
	[SYS_socketpair] = PLEDGE_RW,
	[SYS_getdents] = PLEDGE_RW,

	[SYS_sendto] = PLEDGE_RW | PLEDGE_DNS_ACTIVE | PLEDGE_YP_ACTIVE,
	[SYS_sendmsg] = PLEDGE_RW,
	[SYS_recvmsg] = PLEDGE_RW,
	[SYS_recvfrom] = PLEDGE_RW | PLEDGE_DNS_ACTIVE | PLEDGE_YP_ACTIVE,

	[SYS_fork] = PLEDGE_PROC,
	[SYS_vfork] = PLEDGE_PROC,
	[SYS_kill] = PLEDGE_PROC,
	[SYS_setpgid] = PLEDGE_PROC,
	[SYS_sigsuspend] = PLEDGE_PROC,
	[SYS_setrlimit] = PLEDGE_PROC,

	[SYS_execve] = PLEDGE_EXEC,

	[SYS_setgroups] = PLEDGE_PROC,
	[SYS_setresgid] = PLEDGE_PROC,
	[SYS_setresuid] = PLEDGE_PROC,

	/* FIONREAD/FIONBIO, plus further checks in pledge_ioctl_check() */
	[SYS_ioctl] = PLEDGE_RW | PLEDGE_IOCTL | PLEDGE_TTY,

	[SYS_getentropy] = PLEDGE_MALLOC,
	[SYS_madvise] = PLEDGE_MALLOC,
	[SYS_minherit] = PLEDGE_MALLOC,
	[SYS_mmap] = PLEDGE_MALLOC,
	[SYS_mprotect] = PLEDGE_MALLOC,
	[SYS_mquery] = PLEDGE_MALLOC,
	[SYS_munmap] = PLEDGE_MALLOC,

	[SYS_open] = PLEDGE_SELF,			/* further checks in namei */
	[SYS_stat] = PLEDGE_SELF,			/* further checks in namei */
	[SYS_access] = PLEDGE_SELF,		/* further checks in namei */
	[SYS_readlink] = PLEDGE_SELF,		/* further checks in namei */

	[SYS_chdir] = PLEDGE_RPATH,
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
	[SYS_chown] = PLEDGE_FATTR,
	[SYS_fchownat] = PLEDGE_FATTR,
	[SYS_lchown] = PLEDGE_FATTR,
	[SYS_fchown] = PLEDGE_FATTR,

	[SYS_socket] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS_ACTIVE | PLEDGE_YP_ACTIVE,
	[SYS_connect] = PLEDGE_INET | PLEDGE_UNIX | PLEDGE_DNS_ACTIVE | PLEDGE_YP_ACTIVE,

	[SYS_listen] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_bind] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_accept4] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_accept] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_getpeername] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_getsockname] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_setsockopt] = PLEDGE_INET | PLEDGE_UNIX,
	[SYS_getsockopt] = PLEDGE_INET | PLEDGE_UNIX,

	[SYS_flock] = PLEDGE_GETPW,
};

static const struct {
	char *name;
	int flags;
} pledgereq[] = {
	{ "malloc",		PLEDGE_SELF | PLEDGE_MALLOC },
	{ "rw",			PLEDGE_SELF | PLEDGE_RW },
	{ "stdio",		PLEDGE_SELF | PLEDGE_MALLOC | PLEDGE_RW },
	{ "rpath",		PLEDGE_SELF | PLEDGE_RW | PLEDGE_RPATH },
	{ "wpath",		PLEDGE_SELF | PLEDGE_RW | PLEDGE_WPATH },
	{ "tmppath",		PLEDGE_SELF | PLEDGE_RW | PLEDGE_TMPPATH },
	{ "inet",		PLEDGE_SELF | PLEDGE_RW | PLEDGE_INET },
	{ "unix",		PLEDGE_SELF | PLEDGE_RW | PLEDGE_UNIX },
	{ "dns",		PLEDGE_SELF | PLEDGE_MALLOC | PLEDGE_DNSPATH },
	{ "getpw",		PLEDGE_SELF | PLEDGE_MALLOC | PLEDGE_RW | PLEDGE_GETPW },
/*X*/	{ "cmsg",		PLEDGE_UNIX | PLEDGE_INET | PLEDGE_SENDFD | PLEDGE_RECVFD },
	{ "sendfd",		PLEDGE_RW | PLEDGE_SENDFD },
	{ "recvfd",		PLEDGE_RW | PLEDGE_RECVFD },
	{ "ioctl",		PLEDGE_IOCTL },
	{ "route",		PLEDGE_ROUTE },
	{ "mcast",		PLEDGE_MCAST },
	{ "tty",		PLEDGE_TTY },
	{ "proc",		PLEDGE_PROC },
	{ "exec",		PLEDGE_EXEC },
	{ "cpath",		PLEDGE_CPATH },
	{ "abort",		PLEDGE_ABORT },
	{ "fattr",		PLEDGE_FATTR },
	{ "prot_exec",		PLEDGE_PROTEXEC },
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
	p->p_p->ps_flags |= PS_PLEDGE;
	return (0);
}

int
pledge_check(struct proc *p, int code)
{
	p->p_pledgenote = p->p_pledgeafter = 0;	/* XX optimise? */
	p->p_pledge_syscall = code;

	if (code < 0 || code > SYS_MAXSYSCALL - 1)
		return (0);

	if (p->p_p->ps_pledge == 0)
		return (code == SYS_exit || code == SYS_kbind);
	return (p->p_p->ps_pledge & pledge_syscalls[code]);
}

int
pledge_fail(struct proc *p, int error, int code)
{
	printf("%s(%d): syscall %d\n", p->p_comm, p->p_pid, p->p_pledge_syscall);
	if (p->p_p->ps_pledge & PLEDGE_ABORT) {	/* Core dump requested */
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

	if (p->p_pledgenote == TMN_COREDUMP)
		return (0);			/* Allow a coredump */

	if (canonpath(origpath, path, sizeof(path)) != 0)
		return (pledge_fail(p, EPERM, PLEDGE_RPATH));

	if ((p->p_pledgenote & TMN_FATTR) &&
	    (p->p_p->ps_pledge & PLEDGE_FATTR) == 0) {
		printf("%s(%d): inode syscall%d, not allowed\n",
		    p->p_comm, p->p_pid, p->p_pledge_syscall);
		return (pledge_fail(p, EPERM, PLEDGE_FATTR));
	}

	/* Detect what looks like a mkstemp(3) family operation */
	if ((p->p_p->ps_pledge & PLEDGE_TMPPATH) &&
	    (p->p_pledge_syscall == SYS_open) &&
	    (p->p_pledgenote & TMN_CPATH) &&
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
	if ((p->p_pledgenote & TMN_CPATH) &&
	    ((p->p_p->ps_pledge & PLEDGE_CPATH) == 0))
		return (pledge_fail(p, EPERM, PLEDGE_CPATH));

	if ((p->p_pledgenote & TMN_WPATH) &&
	    (p->p_p->ps_pledge & PLEDGE_WPATH) == 0)
		return (pledge_fail(p, EPERM, PLEDGE_WPATH));

	/* Read-only paths used occasionally by libc */
	switch (p->p_pledge_syscall) {
	case SYS_access:
		/* tzset() needs this. */
		if ((p->p_pledgenote == TMN_RPATH) &&
		    strcmp(path, "/etc/localtime") == 0)
			return (0);
		break;
	case SYS_open:
		/* getpw* and friends need a few files */
		if ((p->p_pledgenote == TMN_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_GETPW)) {
			if (strcmp(path, "/etc/spwd.db") == 0)
				return (EPERM);
			if (strcmp(path, "/etc/pwd.db") == 0)
				return (0);
			if (strcmp(path, "/etc/group") == 0)
				return (0);
		}

		/* DNS needs /etc/{resolv.conf,hosts,services}. */
		if ((p->p_pledgenote == TMN_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_DNSPATH)) {
			if (strcmp(path, "/etc/resolv.conf") == 0) {
				p->p_pledgeafter |= TMA_DNSRESOLV;
				return (0);
			}
			if (strcmp(path, "/etc/hosts") == 0)
				return (0);
			if (strcmp(path, "/etc/services") == 0)
				return (0);
		}
		if ((p->p_pledgenote == TMN_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_GETPW)) {
			if (strcmp(path, "/var/run/ypbind.lock") == 0) {
				p->p_pledgeafter |= TMA_YPLOCK;
				return (0);
			}
			if (strncmp(path, "/var/yp/binding/",
			    sizeof("/var/yp/binding/") - 1) == 0)
				return (0);
		}
		/* tzset() needs these. */
		if ((p->p_pledgenote == TMN_RPATH) &&
		    strncmp(path, "/usr/share/zoneinfo/",
		    sizeof("/usr/share/zoneinfo/") - 1) == 0)
			return (0);
		if ((p->p_pledgenote == TMN_RPATH) &&
		    strcmp(path, "/etc/localtime") == 0)
			return (0);

		/* /usr/share/nls/../libc.cat has to succeed for strerror(3). */
		if ((p->p_pledgenote == TMN_RPATH) &&
		    strncmp(path, "/usr/share/nls/",
		    sizeof("/usr/share/nls/") - 1) == 0 &&
		    strcmp(path + strlen(path) - 9, "/libc.cat") == 0)
			return (0);
		break;
	case SYS_readlink:
		/* Allow /etc/malloc.conf for malloc(3). */
		if ((p->p_pledgenote == TMN_RPATH) &&
		    strcmp(path, "/etc/malloc.conf") == 0)
			return (0);
		break;
	case SYS_stat:
		/* DNS needs /etc/resolv.conf. */
		if ((p->p_pledgenote == TMN_RPATH) &&
		    (p->p_p->ps_pledge & PLEDGE_DNSPATH)) {
			if (strcmp(path, "/etc/resolv.conf") == 0) {
				p->p_pledgeafter |= TMA_DNSRESOLV;
				return (0);
			}
		}
		break;
	}

	/*
	 * If a whitelist is set, compare canonical paths.  Anything
	 * not on the whitelist gets ENOENT.
	 */
	if (p->p_p->ps_pledgepaths) {
		struct whitepaths *wl = p->p_p->ps_pledgepaths;
		char *fullpath, *builtpath = NULL, *canopath = NULL;
		size_t builtlen = 0;
		int i, error;

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
			return (pledge_fail(p, EPERM, PLEDGE_RPATH));
		}

		//printf("namei: canopath = %s strlen %lld\n", canopath,
		//    (long long)strlen(canopath));

		error = ENOENT;
		for (i = 0; i < wl->wl_count && wl->wl_paths[i].name && error; i++) {
			if (strncmp(canopath, wl->wl_paths[i].name,
			    wl->wl_paths[i].len - 1) == 0) {
				u_char term = canopath[wl->wl_paths[i].len - 1];

				if (term == '\0' || term == '/' ||
				    wl->wl_paths[i].name[1] == '\0')
					error = 0;
			}
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

	return (pledge_fail(p, EPERM, PLEDGE_RPATH));
}

void
pledge_aftersyscall(struct proc *p, int code, int error)
{
	if ((p->p_pledgeafter & TMA_YPLOCK) && error == 0)
		atomic_setbits_int(&p->p_p->ps_pledge, PLEDGE_YP_ACTIVE | PLEDGE_INET);
	if ((p->p_pledgeafter & TMA_DNSRESOLV) && error == 0)
		atomic_setbits_int(&p->p_p->ps_pledge, PLEDGE_DNS_ACTIVE);
}

/*
 * By default, only the advisory cmsg's can be received from the kernel,
 * such as TIMESTAMP ntpd.
 *
 * If PLEDGE_RECVFD is set SCM_RIGHTS is also allowed in for a carefully
 * selected set of descriptors (specifically to exclude directories).
 *
 * This results in a kill upon recv, if some other process on the system
 * send a SCM_RIGHTS to an open socket of some sort.  That will discourage
 * leaving such sockets lying around...
 */
int
pledge_cmsg_recv(struct proc *p, struct mbuf *control)
{
	struct msghdr tmp;
	struct cmsghdr *cmsg;
	int *fdp, fd;
	struct file *fp;
	int nfds, i;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	/* Scan the cmsg */
	memset(&tmp, 0, sizeof(tmp));
	tmp.msg_control = mtod(control, struct cmsghdr *);
	tmp.msg_controllen = control->m_len;
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

	if ((p->p_p->ps_pledge & PLEDGE_RECVFD) == 0)
		return pledge_fail(p, EPERM, PLEDGE_RECVFD);

	/* In OpenBSD, a CMSG only contains one SCM_RIGHTS.  Check it. */
	fdp = (int *)CMSG_DATA(cmsg);
	nfds = (cmsg->cmsg_len - CMSG_ALIGN(sizeof(*cmsg))) /
	    sizeof(struct file *);
	for (i = 0; i < nfds; i++) {
		struct vnode *vp;

		fd = *fdp++;
		fp = fd_getfile(p->p_fd, fd);
		if (fp == NULL)
			return pledge_fail(p, EBADF, PLEDGE_RECVFD);

		/* Only allow passing of sockets, pipes, and pure files */
		switch (fp->f_type) {
		case DTYPE_SOCKET:
		case DTYPE_PIPE:
			continue;
		case DTYPE_VNODE:
			vp = (struct vnode *)fp->f_data;
			if (vp->v_type == VREG)
				continue;
			break;
		default:
			break;
		}
		return pledge_fail(p, EPERM, PLEDGE_RECVFD);
	}
	return (0);
}

/*
 * When pledged, default prevents sending of a cmsg.
 *
 * Unlike pledge_cmsg_recv pledge_cmsg_send is called with individual
 * cmsgs one per mbuf. So no need to loop or scan.
 */
int
pledge_cmsg_send(struct proc *p, struct mbuf *control)
{
	struct cmsghdr *cmsg;
	int *fdp, fd;
	struct file *fp;
	int nfds, i;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_SENDFD) == 0)
		return pledge_fail(p, EPERM, PLEDGE_SENDFD);

	/* Scan the cmsg */
	cmsg = mtod(control, struct cmsghdr *);

	/* Contains no SCM_RIGHTS, so OK */
	if (!(cmsg->cmsg_level == SOL_SOCKET &&
	    cmsg->cmsg_type == SCM_RIGHTS))
		return (0);

	/* In OpenBSD, a CMSG only contains one SCM_RIGHTS.  Check it. */
	fdp = (int *)CMSG_DATA(cmsg);
	nfds = (cmsg->cmsg_len - CMSG_ALIGN(sizeof(*cmsg))) /
	    sizeof(struct file *);
	for (i = 0; i < nfds; i++) {
		struct vnode *vp;

		fd = *fdp++;
		fp = fd_getfile(p->p_fd, fd);
		if (fp == NULL)
			return pledge_fail(p, EBADF, PLEDGE_SENDFD);

		/* Only allow passing of sockets, pipes, and pure files */
		switch (fp->f_type) {
		case DTYPE_SOCKET:
		case DTYPE_PIPE:
			continue;
		case DTYPE_VNODE:
			vp = (struct vnode *)fp->f_data;
			if (vp->v_type == VREG)
				continue;
			break;
		default:
			break;
		}
		/* Not allowed to send a bad fd type */
		return pledge_fail(p, EPERM, PLEDGE_SENDFD);
	}
	return (0);
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
		if (miblen == 7 &&
		    mib[0] == CTL_NET && mib[1] == PF_ROUTE &&
		    mib[2] == 0 &&
		    (mib[3] == 0 || mib[3] == AF_INET6 || mib[3] == AF_INET) &&
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
		    mib[2] == 0 && mib[3] == AF_INET &&
		    mib[4] == NET_RT_FLAGS && mib[5] == RTF_LLINFO)
			return (0);
	}

	if ((p->p_p->ps_pledge & (PLEDGE_ROUTE | PLEDGE_INET))) {
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
	if (miblen == 2 &&			/* uname() */
	    mib[0] == CTL_HW && mib[1] == HW_MACHINE)
		return (0);
	if (miblen == 2 &&			/* getpagesize() */
	    mib[0] == CTL_HW && mib[1] == HW_PAGESIZE)
		return (0);
	if (miblen == 2 &&			/* setproctitle() */
	    mib[0] == CTL_VM && mib[1] == VM_PSSTRINGS)
		return (0);

	printf("%s(%d): sysctl %d: %d %d %d %d %d %d\n",
	    p->p_comm, p->p_pid, miblen, mib[0], mib[1],
	    mib[2], mib[3], mib[4], mib[5]);
	return (EFAULT);
}

int
pledge_adjtime_check(struct proc *p, const void *v)
{
	const struct timeval *delta = v;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if (delta)
		return (EFAULT);
	return (0);
}

int
pledge_connect_check(struct proc *p)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_DNS_ACTIVE))
		return (0);	/* A port check happens inside sys_connect() */

	if ((p->p_p->ps_pledge & (PLEDGE_INET | PLEDGE_UNIX)))
		return (0);
	return (EPERM);
}

int
pledge_recvfrom_check(struct proc *p, void *v)
{
	struct sockaddr *from = v;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_DNS_ACTIVE) && from == NULL)
		return (0);
	if (p->p_p->ps_pledge & PLEDGE_INET)
		return (0);
	if (p->p_p->ps_pledge & PLEDGE_UNIX)
		return (0);
	if (from == NULL)
		return (0);		/* behaves just like write */
	return (EPERM);
}

int
pledge_sendto_check(struct proc *p, const void *v)
{
	const struct sockaddr *to = v;

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_DNS_ACTIVE) && to == NULL)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_INET))
		return (0);
	if ((p->p_p->ps_pledge & PLEDGE_UNIX))
		return (0);
	if (to == NULL)
		return (0);		/* behaves just like write */
	return (EPERM);
}

int
pledge_socket_check(struct proc *p, int domain)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);
	if ((p->p_p->ps_pledge & (PLEDGE_INET | PLEDGE_UNIX)))
		return (0);
	if ((p->p_p->ps_pledge & PLEDGE_DNS_ACTIVE) &&
	    (domain == AF_INET || domain == AF_INET6))
		return (0);
	return (EPERM);
}

int
pledge_bind_check(struct proc *p, const void *v)
{

	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);
	if ((p->p_p->ps_pledge & PLEDGE_INET))
		return (0);
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
		return (0);
	}

	if (fp == NULL)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;

	/*
	 * Further sets of ioctl become available, but are checked a
	 * bit more carefully against the vnode.
	 */
	if ((p->p_p->ps_pledge & PLEDGE_IOCTL)) {
		switch (com) {
		case FIOCLEX:
		case FIONCLEX:
		case FIOASYNC:
		case FIOSETOWN:
		case FIOGETOWN:
			return (0);
		case TIOCGETA:
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			return (ENOTTY);
		case TIOCGPGRP:
		case TIOCGWINSZ:	/* various programs */
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			break;
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

	if ((p->p_p->ps_pledge & PLEDGE_ROUTE)) {
		switch (com) {
		case SIOCGIFADDR:
		case SIOCGIFFLAGS:
		case SIOCGIFRDOMAIN:
			if (fp->f_type == DTYPE_SOCKET)
				return (0);
			break;
		}
	}

	if ((p->p_p->ps_pledge & PLEDGE_TTY)) {
		switch (com) {
		case TIOCSPGRP:
			if ((p->p_p->ps_pledge & PLEDGE_PROC) == 0)
				break;
			/* FALTHROUGH */
		case TIOCGETA:
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			return (ENOTTY);
		case TIOCGPGRP:
		case TIOCGWINSZ:	/* various programs */
#if notyet
		case TIOCSTI:		/* ksh? csh? */
#endif
		case TIOCSBRK:		/* cu */
		case TIOCCDTR:		/* cu */
		case TIOCSETA:		/* cu, ... */
		case TIOCSETAW:		/* cu, ... */
		case TIOCSETAF:		/* tcsetattr TCSAFLUSH, script */
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			break;
		}
	}

	return pledge_fail(p, EPERM, PLEDGE_IOCTL);
}

int
pledge_setsockopt_check(struct proc *p, int level, int optname)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	/* common case for PLEDGE_UNIX and PLEDGE_INET */
	switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_RTABLE:
			return (EPERM);
		}
		return (0);
	}

	if ((p->p_p->ps_pledge & PLEDGE_INET) == 0)
		return (EPERM);

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
		case IP_TOS:
		case IP_TTL:
		case IP_MINTTL:
		case IP_PORTRANGE:
		case IP_RECVDSTADDR:
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
pledge_dns_check(struct proc *p, in_port_t port)
{
	if ((p->p_p->ps_flags & PS_PLEDGE) == 0)
		return (0);

	if ((p->p_p->ps_pledge & PLEDGE_INET))
		return (0);
	if ((p->p_p->ps_pledge & PLEDGE_DNS_ACTIVE) && port == htons(53))
		return (0);	/* Allow a DNS connect outbound */
	return (EPERM);
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
