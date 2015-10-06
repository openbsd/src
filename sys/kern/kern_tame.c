/*	$OpenBSD: kern_tame.c,v 1.61 2015/10/06 14:55:41 claudio Exp $	*/

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

	[SYS_getuid] = TAME_SELF,
	[SYS_geteuid] = TAME_SELF,
	[SYS_getresuid] = TAME_SELF,
	[SYS_getgid] = TAME_SELF,
	[SYS_getegid] = TAME_SELF,
	[SYS_getresgid] = TAME_SELF,
	[SYS_getgroups] = TAME_SELF,
	[SYS_getlogin] = TAME_SELF,
	[SYS_getpgrp] = TAME_SELF,
	[SYS_getpgid] = TAME_SELF,
	[SYS_getppid] = TAME_SELF,
	[SYS_getsid] = TAME_SELF,
	[SYS_getthrid] = TAME_SELF,
	[SYS_getrlimit] = TAME_SELF,
	[SYS_gettimeofday] = TAME_SELF,
	[SYS_getdtablecount] = TAME_SELF,
	[SYS_getrusage] = TAME_SELF,
	[SYS_issetugid] = TAME_SELF,
	[SYS_clock_getres] = TAME_SELF,
	[SYS_clock_gettime] = TAME_SELF,
	[SYS_getpid] = TAME_SELF,
	[SYS_umask] = TAME_SELF,
	[SYS_sysctl] = TAME_SELF,	/* read-only; narrow subset */
	[SYS_adjtime] = TAME_SELF,	/* read-only */

	[SYS_fchdir] = TAME_SELF,	/* careful of directory fd inside jails */

	/* needed by threaded programs */
	[SYS_sched_yield] = TAME_SELF,
	[SYS___thrsleep] = TAME_SELF,
	[SYS___thrwakeup] = TAME_SELF,
	[SYS___threxit] = TAME_SELF,
	[SYS___thrsigdivert] = TAME_SELF,

	[SYS_sendsyslog] = TAME_SELF,
	[SYS_nanosleep] = TAME_SELF,
	[SYS_sigprocmask] = TAME_SELF,
	[SYS_sigaction] = TAME_SELF,
	[SYS_sigreturn] = TAME_SELF,
	[SYS_getitimer] = TAME_SELF,
	[SYS_setitimer] = TAME_SELF,

	[SYS_tame] = TAME_SELF,

	[SYS_wait4] = TAME_SELF,

	[SYS_poll] = TAME_RW,
	[SYS_kevent] = TAME_RW,
	[SYS_kqueue] = TAME_RW,
	[SYS_select] = TAME_RW,

	[SYS_close] = TAME_RW,
	[SYS_dup] = TAME_RW,
	[SYS_dup2] = TAME_RW,
	[SYS_dup3] = TAME_RW,
	[SYS_closefrom] = TAME_RW,
	[SYS_shutdown] = TAME_RW,
	[SYS_read] = TAME_RW,
	[SYS_readv] = TAME_RW,
	[SYS_pread] = TAME_RW,
	[SYS_preadv] = TAME_RW,
	[SYS_write] = TAME_RW,
	[SYS_writev] = TAME_RW,
	[SYS_pwrite] = TAME_RW,
	[SYS_pwritev] = TAME_RW,
	[SYS_ftruncate] = TAME_RW,
	[SYS_lseek] = TAME_RW,
	[SYS_fstat] = TAME_RW,

	[SYS_fcntl] = TAME_RW,
	[SYS_fsync] = TAME_RW,
	[SYS_pipe] = TAME_RW,
	[SYS_pipe2] = TAME_RW,
	[SYS_socketpair] = TAME_RW,
	[SYS_getdents] = TAME_RW,

	[SYS_sendto] = TAME_RW | TAME_DNS_ACTIVE | TAME_YP_ACTIVE,
	[SYS_sendmsg] = TAME_RW,
	[SYS_recvmsg] = TAME_RW,
	[SYS_recvfrom] = TAME_RW | TAME_DNS_ACTIVE | TAME_YP_ACTIVE,

	[SYS_fork] = TAME_PROC,
	[SYS_vfork] = TAME_PROC,
	[SYS_kill] = TAME_PROC,

	[SYS_setgroups] = TAME_PROC,
	[SYS_setresgid] = TAME_PROC,
	[SYS_setresuid] = TAME_PROC,

	/* FIONREAD/FIONBIO, plus further checks in tame_ioctl_check() */
	[SYS_ioctl] = TAME_RW | TAME_IOCTL,

	[SYS_getentropy] = TAME_MALLOC,
	[SYS_madvise] = TAME_MALLOC,
	[SYS_minherit] = TAME_MALLOC,
	[SYS_mmap] = TAME_MALLOC,
	[SYS_mprotect] = TAME_MALLOC,
	[SYS_mquery] = TAME_MALLOC,
	[SYS_munmap] = TAME_MALLOC,

	[SYS_open] = TAME_SELF,			/* further checks in namei */
	[SYS_stat] = TAME_SELF,			/* further checks in namei */
	[SYS_access] = TAME_SELF,		/* further checks in namei */
	[SYS_readlink] = TAME_SELF,		/* further checks in namei */

	[SYS_chdir] = TAME_RPATH,
	[SYS_openat] = TAME_RPATH | TAME_WPATH,
	[SYS_fstatat] = TAME_RPATH | TAME_WPATH,
	[SYS_faccessat] = TAME_RPATH | TAME_WPATH,
	[SYS_readlinkat] = TAME_RPATH | TAME_WPATH,
	[SYS_lstat] = TAME_RPATH | TAME_WPATH | TAME_TMPPATH,
	[SYS_rename] = TAME_CPATH,
	[SYS_rmdir] = TAME_CPATH,
	[SYS_renameat] = TAME_CPATH,
	[SYS_link] = TAME_CPATH,
	[SYS_linkat] = TAME_CPATH,
	[SYS_symlink] = TAME_CPATH,
	[SYS_unlink] = TAME_CPATH | TAME_TMPPATH,
	[SYS_unlinkat] = TAME_CPATH,
	[SYS_mkdir] = TAME_CPATH,
	[SYS_mkdirat] = TAME_CPATH,

	/*
	 * Classify as RPATH|WPATH, because of path information leakage.
	 * WPATH due to unknown use of mk*temp(3) on non-/tmp paths..
	 */
	[SYS___getcwd] = TAME_RPATH | TAME_WPATH,

	/* Classify as RPATH, because these leak path information */
	[SYS_getfsstat] = TAME_RPATH,
	[SYS_statfs] = TAME_RPATH,
	[SYS_fstatfs] = TAME_RPATH,

	[SYS_utimes] = TAME_FATTR,
	[SYS_futimes] = TAME_FATTR,
	[SYS_utimensat] = TAME_FATTR,
	[SYS_futimens] = TAME_FATTR,
	[SYS_chmod] = TAME_FATTR,
	[SYS_fchmod] = TAME_FATTR,
	[SYS_fchmodat] = TAME_FATTR,
	[SYS_chflags] = TAME_FATTR,
	[SYS_chflagsat] = TAME_FATTR,
	[SYS_chown] = TAME_FATTR,
	[SYS_fchownat] = TAME_FATTR,
	[SYS_lchown] = TAME_FATTR,
	[SYS_fchown] = TAME_FATTR,

	[SYS_socket] = TAME_INET | TAME_UNIX | TAME_DNS_ACTIVE | TAME_YP_ACTIVE,
	[SYS_connect] = TAME_INET | TAME_UNIX | TAME_DNS_ACTIVE | TAME_YP_ACTIVE,

	[SYS_listen] = TAME_INET | TAME_UNIX,
	[SYS_bind] = TAME_INET | TAME_UNIX,
	[SYS_accept4] = TAME_INET | TAME_UNIX,
	[SYS_accept] = TAME_INET | TAME_UNIX,
	[SYS_getpeername] = TAME_INET | TAME_UNIX,
	[SYS_getsockname] = TAME_INET | TAME_UNIX,
	[SYS_setsockopt] = TAME_INET | TAME_UNIX,
	[SYS_getsockopt] = TAME_INET | TAME_UNIX,

	[SYS_flock] = TAME_GETPW,
};

static const struct {
	char *name;
	int flags;
} tamereq[] = {
	{ "malloc",		TAME_SELF | TAME_MALLOC },
	{ "rw",			TAME_SELF | TAME_RW },
	{ "stdio",		TAME_SELF | TAME_MALLOC | TAME_RW },
	{ "rpath",		TAME_SELF | TAME_RW | TAME_RPATH },
	{ "wpath",		TAME_SELF | TAME_RW | TAME_WPATH },
	{ "tmppath",		TAME_SELF | TAME_RW | TAME_TMPPATH },
	{ "inet",		TAME_SELF | TAME_RW | TAME_INET },
	{ "unix",		TAME_SELF | TAME_RW | TAME_UNIX },
	{ "cmsg",		TAME_SELF | TAME_RW | TAME_UNIX | TAME_CMSG },
	{ "dns",		TAME_SELF | TAME_MALLOC | TAME_DNSPATH },
	{ "ioctl",		TAME_IOCTL },
	{ "getpw",		TAME_SELF | TAME_MALLOC | TAME_RW | TAME_GETPW },
	{ "proc",		TAME_PROC },
	{ "cpath",		TAME_CPATH },
	{ "abort",		TAME_ABORT },
	{ "fattr",		TAME_FATTR },
	{ "prot_exec",		TAME_PROTEXEC },
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
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrstruct(p, "tamereq", rbuf, rbuflen-1);
#endif

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
				free(rbuf, M_TEMP, MAXPATHLEN);
				return (EINVAL);
			}
			flags |= f;
		}
		free(rbuf, M_TEMP, MAXPATHLEN);
	}

	if (flags & ~TAME_USERSET)
		return (EINVAL);

	if ((p->p_p->ps_flags & PS_TAMED)) {
		/* Already tamed, only allow reductions */
		if (((flags | p->p_p->ps_tame) & TAME_USERSET) !=
		    (p->p_p->ps_tame & TAME_USERSET)) {
			return (EPERM);
		}

		flags &= p->p_p->ps_tame;
		flags &= TAME_USERSET;		/* Relearn _ACTIVE */
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
#ifdef KTRACE
			if (KTRPOINT(p, KTR_STRUCT))
				ktrstruct(p, "tamepath", path, len-1);
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
			for (i = 0; i < wl->wl_count; i++)
				free(wl->wl_paths[i].name,
				    M_TEMP, wl->wl_paths[i].len);
			free(wl, M_TEMP, wl->wl_size);
			return (error);
		}
		p->p_p->ps_tamepaths = wl;
#if 0
		printf("tame: %s(%d): paths loaded:\n", p->p_comm, p->p_pid);
		for (i = 0; i < wl->wl_count; i++)
			if (wl->wl_paths[i].name)
				printf("tame: %d=%s %lld\n", i, wl->wl_paths[i].name,
				    (long long)wl->wl_paths[i].len);
#endif
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
	if (p->p_p->ps_tame & TAME_ABORT) {	/* Core dump requested */
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
	    (p->p_p->ps_tame & TAME_FATTR) == 0) {
		printf("%s(%d): inode syscall%d, not allowed\n",
		    p->p_comm, p->p_pid, p->p_tame_syscall);
		return (tame_fail(p, EPERM, TAME_FATTR));
	}

	/* Detect what looks like a mkstemp(3) family operation */
	if ((p->p_p->ps_tame & TAME_TMPPATH) &&
	    (p->p_tame_syscall == SYS_open) &&
	    (p->p_tamenote & TMN_CPATH) &&
	    strncmp(path, "/tmp/", sizeof("/tmp/") - 1) == 0) {
		return (0);
	}

	/* Allow unlinking of a mkstemp(3) file...
	 * Good opportunity for strict checks here.
	 */
	if ((p->p_p->ps_tame & TAME_TMPPATH) &&
	    (p->p_tame_syscall == SYS_unlink) &&
	    strncmp(path, "/tmp/", sizeof("/tmp/") - 1) == 0) {
		return (0);
	}

	/* open, mkdir, or other path creation operation */
	if ((p->p_tamenote & TMN_CPATH) &&
	    ((p->p_p->ps_tame & TAME_CPATH) == 0))
		return (tame_fail(p, EPERM, TAME_CPATH));

	if ((p->p_tamenote & TMN_WPATH) &&
	    (p->p_p->ps_tame & TAME_WPATH) == 0)
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
		    (p->p_p->ps_tame & TAME_GETPW)) {
			if (strcmp(path, "/etc/spwd.db") == 0)
				return (EPERM);
			if (strcmp(path, "/etc/pwd.db") == 0)
				return (0);
			if (strcmp(path, "/etc/group") == 0)
				return (0);
		}

		/* DNS needs /etc/{resolv.conf,hosts,services}. */
		if ((p->p_tamenote == TMN_RPATH) &&
		    (p->p_p->ps_tame & TAME_DNSPATH)) {
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
		    (p->p_p->ps_tame & TAME_GETPW)) {
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
		    (p->p_p->ps_tame & TAME_DNSPATH)) {
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

				if (term == '\0' || term == '/' ||
				    wl->wl_paths[i].name[1] == '\0')
					error = 0;
			}
		}
		free(canopath, M_TEMP, MAXPATHLEN);
		return (error);			/* Don't hint why it failed */
	}

	if (p->p_p->ps_tame & TAME_RPATH)
		return (0);
	if (p->p_p->ps_tame & TAME_WPATH)
		return (0);
	if (p->p_p->ps_tame & TAME_CPATH)
		return (0);

	return (tame_fail(p, EPERM, TAME_RPATH));
}

void
tame_aftersyscall(struct proc *p, int code, int error)
{
	if ((p->p_tameafter & TMA_YPLOCK) && error == 0)
		atomic_setbits_int(&p->p_p->ps_tame, TAME_YP_ACTIVE | TAME_INET);
	if ((p->p_tameafter & TMA_DNSRESOLV) && error == 0)
		atomic_setbits_int(&p->p_p->ps_tame, TAME_DNS_ACTIVE);
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
tame_cmsg_recv(struct proc *p, struct mbuf *control)
{
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

	if ((p->p_p->ps_tame & TAME_CMSG) == 0)
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
		return tame_fail(p, EPERM, TAME_CMSG);
	}
	return (0);
}

/*
 * When tamed, default prevents sending of a cmsg.
 *
 * Unlike tame_cmsg_recv tame_cmsg_send is called with individual
 * cmsgs one per mbuf. So no need to loop or scan.
 */
int
tame_cmsg_send(struct proc *p, struct mbuf *control)
{
	struct cmsghdr *cmsg;
	int *fdp, fd;
	struct file *fp;
	int nfds, i;

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if ((p->p_p->ps_tame & TAME_CMSG) == 0)
		return tame_fail(p, EPERM, TAME_CMSG);

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
			return tame_fail(p, EBADF, TAME_CMSG);

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

	/* setproctitle() */
	if (namelen == 2 &&
	    name[0] == CTL_VM &&
	    name[1] == VM_PSSTRINGS)
		return (0);

	/* getifaddrs() */
	if ((p->p_p->ps_tame & TAME_INET) &&
	    namelen == 6 &&
	    name[0] == CTL_NET && name[1] == PF_ROUTE &&
	    name[2] == 0 && name[3] == 0 &&
	    name[4] == NET_RT_IFLIST && name[5] == 0)
		return (0);

	/* used by arp(8).  Exposes MAC addresses known on local nets */
	/* XXX Put into a special catagory. */
	if ((p->p_p->ps_tame & TAME_INET) &&
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

	if ((p->p_p->ps_tame & TAME_DNS_ACTIVE))
		return (0);	/* A port check happens inside sys_connect() */

	if ((p->p_p->ps_tame & (TAME_INET | TAME_UNIX)))
		return (0);
	return (EPERM);
}

int
tame_recvfrom_check(struct proc *p, void *v)
{
	struct sockaddr *from = v;

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);

	if ((p->p_p->ps_tame & TAME_DNS_ACTIVE) && from == NULL)
		return (0);
	if (p->p_p->ps_tame & TAME_INET)
		return (0);
	if (p->p_p->ps_tame & TAME_UNIX)
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

	if ((p->p_p->ps_tame & TAME_DNS_ACTIVE) && to == NULL)
		return (0);

	if ((p->p_p->ps_tame & TAME_INET))
		return (0);
	if ((p->p_p->ps_tame & TAME_UNIX))
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
	if ((p->p_p->ps_tame & (TAME_INET | TAME_UNIX)))
		return (0);
	if ((p->p_p->ps_tame & TAME_DNS_ACTIVE) &&
	    (domain == AF_INET || domain == AF_INET6))
		return (0);
	return (EPERM);
}

int
tame_bind_check(struct proc *p, const void *v)
{

	if ((p->p_p->ps_flags & PS_TAMED) == 0)
		return (0);
	if ((p->p_p->ps_tame & TAME_INET))
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
	if ((p->p_p->ps_tame & TAME_IOCTL)) {
		switch (com) {
		case FIOCLEX:
		case FIONCLEX:
		case FIOASYNC:
		case FIOSETOWN:
		case FIOGETOWN:
			return (0);
		case TIOCGETA:
		case TIOCGPGRP:
		case TIOCGWINSZ:	/* various programs */
		case TIOCSTI:		/* ksh? csh? */
		case TIOCSBRK:		/* cu */
		case TIOCCDTR:		/* cu */
			if (fp->f_type == DTYPE_VNODE && (vp->v_flag & VISTTY))
				return (0);
			break;
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
			if ((p->p_p->ps_tame & TAME_INET) &&
			    fp->f_type == DTYPE_SOCKET)
				return (0);
			break;
		}
	}

	printf("tame: ioctl %lx\n", com);
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
		break;
	case IPPROTO_IP:
		switch (optname) {
		case IP_TOS:
		case IP_TTL:
		case IP_MINTTL:
		case IP_PORTRANGE:
		case IP_RECVDSTADDR:
			return (0);
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
			return (0);
		}
		break;
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

	if ((p->p_p->ps_tame & TAME_INET))
		return (0);
	if ((p->p_p->ps_tame & TAME_DNS_ACTIVE) && port == htons(53))
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
