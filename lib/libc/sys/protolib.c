/*
 * Copyright (c) 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: protolib.c,v 1.2 1996/08/19 08:34:32 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/ktrace.h>
#include <sys/mount.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "protolib.h"

/*
 * Don't include anything after protolib.h,
 * both LINTLIBRARY and PROTOLIB1 are active.
 */

int	syscall(int, ...);					/*   0 */
void	_exit(int);						/*   1 */
pid_t	fork(void);						/*   2 */
ssize_t	read(int, void *, size_t);				/*   3 */
ssize_t	write(int, const void *, size_t);			/*   4 */
int	open(const char *, int, ...);				/*   5 */
int	close(int);						/*   6 */
pid_t	wait4(pid_t, int *, int, struct rusage *);		/*   7 */

int	link(const char *, const char *);			/*   9 */
int	unlink(const char *);					/*  10 */

int	chdir(const char *);					/*  12 */
int	fchdir(int);						/*  13 */
int	mknod(const char *, mode_t, dev_t);			/*  14 */
int	chmod(const char *, mode_t);				/*  15 */
int	chown(const char *, uid_t, gid_t);			/*  16 */
char	*brk(const char *);					/*  17 */
int	getfsstat(struct statfs *, long, int);			/*  18 */

pid_t	getpid(void);						/*  20 */
int	mount(const char *, const char *, int, void *);		/*  21 */
int	unmount(const char *, int);				/*  22 */
int	setuid(uid_t);						/*  23 */
uid_t	getuid(void);						/*  24 */
uid_t	geteuid(void);						/*  25 */
int	ptrace(int, pid_t, caddr_t, int);			/*  26 */
ssize_t	recvmsg(int, struct msghdr *, int);			/*  27 */
ssize_t	sendmsg(int, const struct msghdr *, int);		/*  28 */
ssize_t	recvfrom(int, void *, size_t, int, struct sockaddr *,	/*  29 */
		 int *);
int	accept(int, struct sockaddr *, int *);			/*  30 */
int	getpeername(int, struct sockaddr *, int *);		/*  31 */
int	getsockname(int, struct sockaddr *, int *);		/*  32 */
int	access(const char *, int);				/*  33 */
int	chflags(const char *, u_long);				/*  34 */
int	fchflags(int, u_long);					/*  35 */
void	sync(void);						/*  36 */
int	kill(pid_t, int);					/*  37 */

pid_t	getppid(void);						/*  39 */

int	dup(int);						/*  41 */
int	pipe(int *);						/*  42 */
gid_t	getegid(void);						/*  43 */
int	profil(char *, size_t, u_long, u_int);			/*  44 */
int	ktrace(const char *, int, int, pid_t);			/*  45 */
int	sigaction(int, const struct sigaction *,		/*  46 */
		  struct sigaction *);
gid_t	getgid(void);						/*  47 */
int	sigprocmask(int, const sigset_t *, sigset_t *);		/*  48 */
int	_getlogin(char *, u_int);				/*  49 */
int	setlogin(const char *);					/*  50 */
int	acct(const char *);					/*  51 */
int	sigpending(sigset_t *);					/*  52 */
int	sigaltstack(const struct sigaltstack *,			/*  53 */
		    struct sigaltstack *);
int	ioctl(int, u_long, ...);				/*  54 */
int	reboot(int);						/*  55 */
int	revoke(const char *);					/*  56 */
int	symlink(const char *, const char *);			/*  57 */
int	readlink(const char *, char *, int);			/*  58 */
int	execve(const char *, char *const [], char *const []);	/*  59 */
mode_t	umask(mode_t);						/*  60 */
int	chroot(const char *);					/*  61 */

int	msync(caddr_t, size_t);					/*  65 */
pid_t	vfork(void);						/*  66 */

char	*sbrk(int);						/*  69 */
char	*sstk(int);						/*  70 */

int	vadvise(int);						/*  72 */
int	munmap(caddr_t, size_t);				/*  73 */
int	mprotect(caddr_t, size_t, int);				/*  74 */
int	madvise(caddr_t, size_t, int);				/*  75 */

int	mincore(caddr_t, size_t, char *);			/*  78 */
int	getgroups(int, gid_t *);				/*  79 */
int	setgroups(int, const gid_t *);				/*  80 */
pid_t	getpgrp(void);						/*  81 */
int	setpgid(pid_t, pid_t);					/*  82 */
int	setitimer(int, const struct itimerval *,		/*  83 */
		  struct itimerval *);

int	swapon(const char *);					/*  85 */
int	getitimer(int, struct itimerval *);			/*  86 */

int	dup2(int, int);						/*  90 */

int	fcntl(int, int, ...);					/*  92 */
int	select(int, fd_set *, fd_set *, fd_set *,		/*  93 */
	       struct timeval *);

int	fsync(int);						/*  95 */
int	setpriority(int, int, int);				/*  96 */
int	socket(int, int, int);					/*  97 */
int	connect(int, const struct sockaddr *, int);		/*  98 */

int	getpriority(int, int);					/* 100 */

int	sigreturn(struct sigcontext *);				/* 103 */
int	bind(int, const struct sockaddr *, int);		/* 104 */
int	setsockopt(int, int, int, const void *, int);		/* 105 */
int	listen(int, int);					/* 106 */

int	sigsuspend(const sigset_t *);				/* 111 */

int	vtrace(int, int);					/* 115 */
int	gettimeofday(struct timeval *, struct timezone *);	/* 116 */
int	getrusage(int, struct rusage *);			/* 117 */
int	getsockopt(int, int, int, void *, int *);		/* 118 */

ssize_t	readv(int, const struct iovec *, int);			/* 120 */
ssize_t	writev(int, const struct iovec *, int);			/* 121 */
int	settimeofday(const struct timeval *,			/* 122 */
		     const struct timezone *);
int	fchown(int, uid_t, gid_t);				/* 123 */
int	fchmod(int, mode_t);					/* 124 */

int	rename(const char *, const char *);			/* 128 */

int	flock(int, int);					/* 131 */
int	mkfifo(const char *, mode_t);				/* 132 */
ssize_t	sendto(int, const void *, size_t, int,			/* 133 */
	       const struct sockaddr *, int);
int	shutdown(int, int);					/* 134 */
int	socketpair(int, int, int, int *);			/* 135 */
int	mkdir(const char *, mode_t);				/* 136 */
int	rmdir(const char *);					/* 137 */
int	utimes(const char *, const struct timeval *);		/* 138 */

int	adjtime(const struct timeval *, struct timeval *);	/* 140 */

pid_t	setsid(void);						/* 147 */
int	quotactl(const char *, int, int, char *);		/* 148 */

int	nfssvc(int, void *);					/* 155 */

int	statfs(const char *, struct statfs *);			/* 157 */
int	fstatfs(int, struct statfs *);				/* 158 */

int	getfh(const char *, fhandle_t *);			/* 161 */

int	sysarch(int, char *);					/* 165 */

int	ntp_adjtime(struct timex *);				/* 176 */
int	ntp_gettime(struct ntptimeval *);			/* 177 */

int	setgid(gid_t);						/* 181 */
int	setegid(gid_t);						/* 182 */
int	seteuid(uid_t);						/* 183 */
int	lfs_bmapv(fsid_t *, struct block_info *, int);		/* 184 */
int	lfs_markv(fsid_t *, struct block_info *, int);		/* 185 */
int	lfs_segclean(fsid_t *, u_long);				/* 186 */
int	lfs_segwait(fsid_t *, struct timeval *);		/* 187 */
int	stat(const char *, struct stat *);			/* 188 */
int	fstat(int, struct stat *);				/* 189 */
int	lstat(const char *, struct stat *);			/* 190 */
long	pathconf(const char *, int);				/* 191 */
long	fpathconf(int, int);					/* 192 */

int	getrlimit(int, struct rlimit *);			/* 194 */
int	setrlimit(int, const struct rlimit *);			/* 195 */
int	getdirentries(int, char *, int, long *);		/* 196 */

quad_t	__syscall(quad_t, ...);					/* 198 */

int	__sysctl(int *, u_int, void *, size_t *, void *,	/* 202 */
		 size_t);
int	mlock(caddr_t, size_t);					/* 203 */
int	munlock(caddr_t, size_t);				/* 204 */
int	undelete(const char *);					/* 205 */

int	__semctl(int, int, int, union semun *);			/* 220 */
int	semget(key_t, int, int);				/* 221 */
int	semop(int, struct sembuf *, u_int);			/* 222 */
int	semconfig(int);						/* 223 */
int	msgctl(int, int, struct msqid_ds *);			/* 224 */
int	msgget(key_t, int);					/* 225 */
int	msgsnd(int, void *, size_t, int);			/* 226 */
int	msgrcv(int, void *, size_t, long, int);			/* 227 */
void	*shmat(int, void *, int);				/* 228 */
int	shmctl(int, int, struct shmid_ds *);			/* 229 */
int	shmdt(void *);						/* 230 */
int	shmget(key_t, int, int);				/* 231 */

int	minherit(caddr_t, size_t, int);				/* 250 */
int	rfork(int);						/* 251 */
