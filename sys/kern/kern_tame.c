/*	$OpenBSD: kern_tame.c,v 1.39 2015/09/11 08:22:31 guenther Exp $	*/

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
#include <sys/tame.h>

int canonpath(const char *input, char *buf, size_t bufsize);

const u_int tame_syscalls[SYS_MAXSYSCALL] = {
	[SYS_exit] = 0xffffffff,
	[SYS_kbind] = 0xffffffff,

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

	[SYS_fchdir] = _TM_SELF,	/* careful of directory fd inside jails */

	/* needed by threaded programs */
	[SYS_sched_yield] = _TM_SELF,
	[SYS___thrsleep] = _TM_SELF,
	[SYS___thrwakeup] = _TM_SELF,
	[SYS___threxit] = _TM_SELF,
	[SYS___thrsigdivert] = _TM_SELF,

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
	[SYS_fstat] = _TM_RW,

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

	[SYS_open] = _TM_SELF,
	[SYS_stat] = _TM_SELF,
	[SYS_access] = _TM_SELF,
	[SYS_readlink] = _TM_SELF,

	[SYS_chdir] = _TM_RPATH,
	[SYS___getcwd] = _TM_RPATH | _TM_WPATH,
	[SYS_openat] = _TM_RPATH | _TM_WPATH,
	[SYS_fstatat] = _TM_RPATH | _TM_WPATH,
	[SYS_faccessat] = _TM_RPATH | _TM_WPATH,
	[SYS_readlinkat] = _TM_RPATH | _TM_WPATH,
	[SYS_lstat] = _TM_RPATH | _TM_WPATH | _TM_TMPPATH | _TM_DNSPATH,
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

	/* Classify so due to info leak */
	[SYS_getfsstat] = _TM_RPATH,
	/* XXX Consider statfs and fstatfs */

	[SYS_utimes] = _TM_FATTR,
	[SYS_futimes] = _TM_FATTR,
	[SYS_utimensat] = _TM_FATTR,
	[SYS_futimens] = _TM_FATTR,
	[SYS_chmod] = _TM_FATTR,
	[SYS_fchmod] = _TM_FATTR,
	[SYS_fchmodat] = _TM_FATTR,
	[SYS_chflags] = _TM_FATTR,
	[SYS_chflagsat] = _TM_FATTR,
	[SYS_chown] = _TM_FATTR,
	[SYS_fchownat] = _TM_FATTR,
	[SYS_lchown] = _TM_FATTR,
	[SYS_fchown] = _TM_FATTR,

	[SYS_socket] = _TM_INET | _TM_UNIX | _TM_DNS_ACTIVE | _TM_YP_ACTIVE,
	[SYS_connect] = _TM_INET | _TM_UNIX | _TM_DNS_ACTIVE | _TM_YP_ACTIVE,

	[SYS_listen] = _TM_INET | _TM_UNIX,
	[SYS_bind] = _TM_INET | _TM_UNIX,
	[SYS_accept4] = _TM_INET | _TM_UNIX,
	[SYS_accept] = _TM_INET | _TM_UNIX,
	[SYS_getpeername] = _TM_INET | _TM_UNIX,
	[SYS_getsockname] = _TM_INET | _TM_UNIX,
	[SYS_setsockopt] = _TM_INET | _TM_UNIX,
	[SYS_getsockopt] = _TM_INET | _TM_UNIX,

	[SYS_flock] = _TM_GETPW,
};

static const struct {
	char *name;
	int flags;
} tamereq[] = {
	{ "malloc",		_TM_SELF | _TM_MALLOC },
	{ "rw",			_TM_SELF | _TM_RW },
	{ "stdio",		_TM_SELF | _TM_MALLOC | _TM_RW },
	{ "rpath",		_TM_SELF | _TM_RW | _TM_RPATH },
	{ "wpath",		_TM_SELF | _TM_RW | _TM_WPATH },
	{ "tmppath",		_TM_SELF | _TM_RW | _TM_TMPPATH },
	{ "inet",		_TM_SELF | _TM_RW | _TM_INET },
	{ "unix",		_TM_SELF | _TM_RW | _TM_UNIX },
	{ "cmsg",		TAME_UNIX | _TM_CMSG },
	{ "dns",		TAME_MALLOC | _TM_DNSPATH },
	{ "ioctl",		_TM_IOCTL },
	{ "getpw",		TAME_STDIO | _TM_GETPW },
	{ "proc",		_TM_PROC },
	{ "cpath",		_TM_CPATH },
	{ "abort",		_TM_ABORT },
	{ "fattr",		_TM_FATTR }
};

int
sys_tame(struct proc *p, void *v, register_t *retval)
{
	struct sys_tame_args /* {
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

		for (rp = rbuf; rp && *rp && error == 0; rp = pn) {
			pn = strchr(rp, ' ');	/* find terminator */
			if (pn) {
				while (*pn == ' ')
					*pn++ = '\0';
			}

			for (f = i = 0; i < nitems(tamereq); i++) {
				if (strcmp(rp, tamereq[i].name) == 0) {
					f = tamereq[i].flags;
					break;
				}
			}
			if (f == 0) {
				printf("%s(%d): unknown req %s\n",
				    p->p_comm, p->p_pid, rp);
				free(rbuf, M_TEMP, MAXPATHLEN);
				return (EINVAL);
			}
			flags |= f;
		}
		free(rbuf, M_TEMP, MAXPATHLEN);
	}

	if (flags & ~_TM_USERSET)
		return (EINVAL);

	if ((p->p_p->ps_flags & PS_TAMED)) {
		/* Already tamed, only allow reductions */
		if (((flags | p->p_p->ps_tame) & _TM_USERSET) !=
		    (p->p_p->ps_tame & _TM_USERSET)) {
			printf("%s(%d): fail change %x %x\n", p->p_comm, p->p_pid,
			    flags, p->p_p->ps_tame);
			return (EPERM);
		}

		flags &= p->p_p->ps_tame;
		flags &= _TM_USERSET;		/* Relearn _ACTIVE */
	}

	if (SCARG(uap, paths)) {
		const char **u = SCARG(uap, paths), *sp;
		struct whitepaths *wl;
		char *cwdpath = NULL, *path;
		size_t cwdpathlen = MAXPATHLEN * 4, cwdlen, len, maxargs = 0;
		int i, error;

		if (p->p_p->ps_tamepaths)
			return (EPERM);

		/* Count paths */
		for (i = 0; i < TAME_MAXPATHS; i++) {
			if ((error = copyin(u + i, &sp, sizeof(sp))) != 0)
				return (error);
			if (sp == NULL)
				break;
		}
		if (i == TAME_MAXPATHS)
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
				// printf("tame: builtpath = %s\n", builtpath);
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

			//printf("tame: canopath = %s %lld strlen %lld\n", canopath,
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
			printf("%s(%d): path load error %d\n",
			    p->p_comm, p->p_pid, error);
			for (i = 0; i < wl->wl_count; i++)
				free(wl->wl_paths[i].name,
				    M_TEMP, wl->wl_paths[i].len);
			free(wl, M_TEMP, wl->wl_size);
			return (error);
		}
		p->p_p->ps_tamepaths = wl;
		printf("tame: %s(%d): paths loaded:\n", p->p_comm, p->p_pid);
		for (i = 0; i < wl->wl_count; i++)
			if (wl->wl_paths[i].name)
				printf("tame: %d=%s %lld\n", i, wl->wl_paths[i].name,
				    (long long)wl->wl_paths[i].len);
	}

	p->p_p->ps_tame = flags;
	p->p_p->ps_flags |= PS_TAMED;
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
		return (code == SYS_exit || code == SYS_kbind);
	return (p->p_p->ps_tame & tame_syscalls[code]);
}

int
tame_fail(struct proc *p, int error, int code)
{
	printf("%s(%d): syscall %d\n", p->p_comm, p->p_pid, p->p_tame_syscall);
	if (p->p_p->ps_tame & _TM_ABORT) {	/* Core dump requested */
		struct sigaction sa;

		memset(&sa, 0, sizeof sa);
		sa.sa_handler = SIG_DFL;
		setsigvec(p, SIGABRT, &sa);
		psignal(p, SIGABRT);
	} else
		psignal(p, SIGKILL);

	p->p_p->ps_tame = 0;		/* Disable all TAME_ flags */
	return (error);
}

/*
 * Need to make it more obvious that one cannot get through here
 * without the right flags set
 */
int
tame_namei(struct proc *p, char *origpath)
{
	char path[PATH_MAX];

	if (p->p_tamenote == TMN_COREDUMP)
		return (0);			/* Allow a coredump */

	if (canonpath(origpath, path, sizeof(path)) != 0)
		return (tame_fail(p, EPERM, TAME_RPATH));

	if ((p->p_tamenote & TMN_FATTR) &&
	    (p->p_p->ps_tame & _TM_FATTR) == 0) {
		printf("%s(%d): inode syscall%d, not allowed\n",
		    p->p_comm, p->p_pid, p->p_tame_syscall);
		return (tame_fail(p, EPERM, TAME_FATTR));
	}

	/* Detect what looks like a mkstemp(3) family operation */
	if ((p->p_p->ps_tame & _TM_TMPPATH) &&
	    (p->p_tame_syscall == SYS_open) &&
	    (p->p_tamenote & TMN_CPATH) &&
	    strncmp(path, "/tmp/", sizeof("/tmp/") - 1) == 0) {
		return (0);
	}

	/* Allow unlinking of a mkstemp(3) file...
	 * Good opportunity for strict checks here.
	 */
	if ((p->p_p->ps_tame & _TM_TMPPATH) &&
	    (p->p_tame_syscall == SYS_unlink) &&
	    strncmp(path, "/tmp/", sizeof("/tmp/") - 1) == 0) {
		return (0);
	}

	/* open, mkdir, or other path creation operation */
	if ((p->p_tamenote & TMN_CPATH) &&
	    ((p->p_p->ps_tame & _TM_CPATH) == 0))
		return (tame_fail(p, EPERM, TAME_CPATH));

	if ((p->p_tamenote & TMN_WPATH) &&
	    (p->p_p->ps_tame & _TM_WPATH) == 0)
		return (tame_fail(p, EPERM, TAME_WPATH));

	/* Read-only paths used occasionally by libc */
	switch (p->p_tame_syscall) {
	case SYS_access:
		/* tzset() needs this. */
		if ((p->p_tamenote == TMN_RPATH) &&
		    strcmp(path, "/etc/localtime") == 0)
			return (0);
		break;
	case SYS_open:
		/* getpw* and friends need a few files */
		if ((p->p_tamenote == TMN_RPATH) &&
		    (p->p_p->ps_tame & _TM_GETPW)) {
			if (strcmp(path, "/etc/spwd.db") == 0)
				return (0);
			if (strcmp(path, "/etc/pwd.db") == 0)
				return (0);
			if (strcmp(path, "/etc/group") == 0)
				return (0);
		}

		/* DNS needs /etc/{resolv.conf,hosts,services}. */
		if ((p->p_tamenote == TMN_RPATH) &&
		    (p->p_p->ps_tame & _TM_DNSPATH)) {
			if (strcmp(path, "/etc/resolv.conf") == 0) {
				p->p_tameafter |= TMA_DNSRESOLV;
				return (0);
			}
			if (strcmp(path, "/etc/hosts") == 0)
				return (0);
			if (strcmp(path, "/etc/services") == 0)
				return (0);
		}
		if ((p->p_tamenote == TMN_RPATH) &&
		    (p->p_p->ps_tame & _TM_GETPW)) {
			if (strcmp(path, "/var/run/ypbind.lock") == 0) {
				p->p_tameafter |= TMA_YPLOCK;
				return (0);
			}
			if (strncmp(path, "/var/yp/binding/",
			    sizeof("/var/yp/binding/") - 1) == 0)
				return (0);
		}
		/* tzset() needs these. */
		if ((p->p_tamenote == TMN_RPATH) &&
		    strncmp(path, "/usr/share/zoneinfo/",
		    sizeof("/usr/share/zoneinfo/") - 1) == 0)
			return (0);
		if ((p->p_tamenote == TMN_RPATH) &&
		    strcmp(path, "/etc/localtime") == 0)
			return (0);

		/* /usr/share/nls/../libc.cat has to succeed for strerror(3). */
		if ((p->p_tamenote == TMN_RPATH) &&
		    strncmp(path, "/usr/share/nls/",
		    sizeof("/usr/share/nls/") - 1) == 0 &&
		    strcmp(path + strlen(path) - 9, "/libc.cat") == 0)
			return (0);
		break;
	case SYS_readlink:
		/* Allow /etc/malloc.conf for malloc(3). */
		if ((p->p_tamenote == TMN_RPATH) &&
		    strcmp(path, "/etc/malloc.conf") == 0)
			return (0);
		break;
	case SYS_stat:
		/* DNS needs /etc/resolv.conf. */
		if ((p->p_tamenote == TMN_RPATH) &&
		    (p->p_p->ps_tame & _TM_DNSPATH)) {
			if (strcmp(path, "/etc/resolv.conf") == 0) {
				p->p_tameafter |= TMA_DNSRESOLV;
				return (0);
			}
		}
		break;
	}

	/*
	 * If a whitelist is set, compare canonical paths.  Anything
	 * not on the whitelist gets ENOENT.
	 */
	if (p->p_p->ps_tamepaths) {
		struct whitepaths *wl = p->p_p->ps_tamepaths;
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
			return (tame_fail(p, EPERM, TAME_RPATH));
		}

		//printf("namei: canopath = %s strlen %lld\n", canopath,
		//    (long long)strlen(canopath));

		error = ENOENT;
		for (i = 0; i < wl->wl_count && wl->wl_paths[i].name && error; i++) {
			if (strncmp(canopath, wl->wl_paths[i].name,
			    wl->wl_paths[i].len - 1) == 0) {
				u_char term = canopath[wl->wl_paths[i].len - 1];

				if (term == '\0' || term == '/')
					error = 0;
			}
		}
		if (error)
			printf("bad path: %s\n", canopath);
		free(canopath, M_TEMP, MAXPATHLEN);
		return (error);			/* Don't hint why it failed */
	}

	if (p->p_p->ps_tame & _TM_RPATH)
		return (0);

	if (p->p_p->ps_tame & _TM_WPATH)
		return (0);

	return (tame_fail(p, EPERM, TAME_RPATH));
}

void
tame_aftersyscall(struct proc *p, int code, int error)
{
	if ((p->p_tameafter & TMA_YPLOCK) && error == 0)
		atomic_setbits_int(&p->p_p->ps_tame, _TM_YP_ACTIVE | TAME_INET);
	if ((p->p_tameafter & TMA_DNSRESOLV) && error == 0)
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
	int *fdp, fd;
	struct file *fp;
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
	fdp = (int *)CMSG_DATA(cmsg);
	nfds = (cmsg->cmsg_len - CMSG_ALIGN(sizeof(*cmsg))) /
	    sizeof(struct file *);
	for (i = 0; i < nfds; i++) {
		struct vnode *vp;

		fd = *fdp++;
		fp = fd_getfile(p->p_fd, fd);
		if (fp == NULL)
			return tame_fail(p, EBADF, TAME_CMSG);

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
 */
int
tame_cmsg_send(struct proc *p, void *v, int controllen)
{
	struct mbuf *control = v;
	struct msghdr tmp;
	struct cmsghdr *cmsg;
	int *fdp, fd;
	struct file *fp;
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
	fdp = (int *)CMSG_DATA(cmsg);
	nfds = (cmsg->cmsg_len - CMSG_ALIGN(sizeof(*cmsg))) /
	    sizeof(struct file *);
	for (i = 0; i < nfds; i++) {
		struct vnode *vp;

		fd = *fdp++;
		fp = fd_getfile(p->p_fd, fd);
		if (fp == NULL)
			return tame_fail(p, EBADF, TAME_CMSG);

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

	/* getdomainname(), gethostname(), getpagesize(), uname() */
	if (namelen == 2 &&
	    name[0] == CTL_KERN && name[1] == KERN_DOMAINNAME)
		return (0);
	if (namelen == 2 &&
	    name[0] == CTL_KERN && name[1] == KERN_HOSTNAME)
		return (0);
	if (namelen == 2 &&
	    name[0] == CTL_KERN && name[1] == KERN_OSTYPE)
		return (0);
	if (namelen == 2 &&
	    name[0] == CTL_KERN && name[1] == KERN_OSRELEASE)
		return (0);
	if (namelen == 2 &&
	    name[0] == CTL_KERN && name[1] == KERN_OSVERSION)
		return (0);
	if (namelen == 2 &&
	    name[0] == CTL_KERN && name[1] == KERN_VERSION)
		return (0);
	if (namelen == 2 &&
	    name[0] == CTL_HW && name[1] == HW_MACHINE)
		return (0);
	if (namelen == 2 &&
	    name[0] == CTL_HW && name[1] == HW_PAGESIZE)
		return (0);

	printf("%s(%d): sysctl %d: %d %d %d %d %d %d\n",
	    p->p_comm, p->p_pid, namelen, name[0], name[1],
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
	struct vnode *vp = NULL;

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if (fp == NULL)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;

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
	case TIOCGETA:
	case TIOCGPGRP:
	case TIOCGWINSZ:	/* various programs */
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
	case BIOCGSTATS:	/* bpf: tcpdump privsep on ^C */
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
