/*	$OpenBSD: unistd.h,v 1.42 2002/09/17 21:15:58 deraadt Exp $ */
/*	$NetBSD: unistd.h,v 1.26.4.1 1996/05/28 02:31:51 mrg Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)unistd.h	5.13 (Berkeley) 6/17/91
 */

#ifndef _UNISTD_H_
#define	_UNISTD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/unistd.h>

#define	STDIN_FILENO	0	/* standard input file descriptor */
#define	STDOUT_FILENO	1	/* standard output file descriptor */
#define	STDERR_FILENO	2	/* standard error file descriptor */

#ifndef NULL
#ifdef 	__GNUG__
#define	NULL	__null
#else
#define	NULL		0	/* null pointer constant */
#endif
#endif

__BEGIN_DECLS
__dead void	 _exit(int);
int	 access(const char *, int);
unsigned int alarm(unsigned int);
int	 chdir(const char *);
int	 chown(const char *, uid_t, gid_t);
int	 close(int);
size_t	 confstr(int, char *, size_t);
char	*cuserid(char *);
int	 dup(int);
int	 dup2(int, int);
int	 execl(const char *, const char *, ...) 
	    __attribute__((sentinel));
int	 execle(const char *, const char *, ...) 
	    __attribute__((sentinel));
int	 execlp(const char *, const char *, ...) 
	    __attribute__((sentinel));
int	 execv(const char *, char * const *);
int	 execve(const char *, char * const *, char * const *);
int	 execvp(const char *, char * const *);
pid_t	 fork(void);
long	 fpathconf(int, int);
char	*getcwd(char *, size_t);
gid_t	 getegid(void);
uid_t	 geteuid(void);
gid_t	 getgid(void);
int	 getgroups(int, gid_t *);
char	*getlogin(void);
int	 getlogin_r(char *, size_t);
pid_t	 getpgrp(void);
pid_t	 getpid(void);
pid_t	 getpgid(pid_t);
pid_t	 getppid(void);
pid_t	 getsid(pid_t);
uid_t	 getuid(void);
int	 isatty(int);
int	 link(const char *, const char *);
off_t	 lseek(int, off_t, int);
long	 pathconf(const char *, int);
int	 pause(void);
int	 pipe(int *);
ssize_t	 read(int, void *, size_t);
int	 rmdir(const char *);
int	 setgid(gid_t);
int	 setpgid(pid_t, pid_t);
pid_t	 setsid(void);
int	 setuid(uid_t);
unsigned int sleep(unsigned int);
long	 sysconf(int);
pid_t	 tcgetpgrp(int);
int	 tcsetpgrp(int, pid_t);
char	*ttyname(int);
int	 ttyname_r(int, char *, size_t);
int	 unlink(const char *);
ssize_t	 write(int, const void *, size_t);

#ifndef	_POSIX_SOURCE

/* structure timeval required for select() */
#include <sys/time.h>

/*
 * X/Open CAE Specification Issue 5 Version 2
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
    (_XOPEN_VERSION - 0) >= 500
ssize_t  pread(int, void *, size_t, off_t);
ssize_t  pwrite(int, const void *, size_t, off_t);
#endif

int	 acct(const char *);
char	*brk(const char *);
int	 chroot(const char *);
char	*crypt(const char *, const char *);
int	 des_cipher(const char *, char *, long, int);
int	 des_setkey(const char *key);
int	 encrypt(char *, int);
void	 endusershell(void);
int	 exect(const char *, char * const *, char * const *);
int	 fchdir(int);
int	 fchown(int, uid_t, gid_t);
char	*fflagstostr(u_int32_t);
int	 fsync(int);
int	 ftruncate(int, off_t);
int	 getdomainname(char *, size_t);
int	 getdtablesize(void);
int	 getgrouplist(const char *, gid_t, gid_t *, int *);
long	 gethostid(void);
int	 gethostname(char *, size_t);
mode_t	 getmode(const void *, mode_t);
int	 getpagesize(void);
char	*getpass(const char *);
char	*getusershell(void);
char	*getwd(char *);			/* obsoleted by getcwd() */
int	 initgroups(const char *, gid_t);
int	 iruserok(u_int32_t, int, const char *, const char *);
int	 iruserok_sa(const void *, int, int, const char *, const char *);
int	 lchown(const char *, uid_t, gid_t);
char	*mkdtemp(char *);
int	 mkstemp(char *);
int	 mkstemps(char *, int);
char	*mktemp(char *);
int	 nfssvc(int, void *);
int	 nice(int);
void	 psignal(unsigned int, const char *);
extern __const char *__const sys_siglist[];
int	 profil(char *, size_t, unsigned long, unsigned int);
int	 rcmd(char **, int, const char *,
	    const char *, const char *, int *);
int	 rcmd_af(char **, int, const char *,
	    const char *, const char *, int *, int);
int	 rcmdsh(char **, int, const char *,
	    const char *, const char *, char *);
char	*re_comp(const char *);
int	 re_exec(const char *);
int	 readlink(const char *, char *, size_t);
int	 reboot(int);
int	 revoke(const char *);
int	 rfork(int opts);
int	 rresvport(int *);
int	 rresvport_af(int *, int);
int	 ruserok(const char *, int, const char *, const char *);
int	 quotactl(const char *, int, int, char *);
char	*sbrk(int);

#if !defined(_XOPEN_SOURCE)
int	 select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#endif

int	 setdomainname(const char *, size_t);
int	 setegid(gid_t);
int	 seteuid(uid_t);
int	 setgroups(int, const gid_t *);
int	 sethostid(long);
int	 sethostname(const char *, size_t);
int	 setkey(const char *);
int	 setlogin(const char *);
void	*setmode(const char *);
int	 setpgrp(pid_t pid, pid_t pgrp);	/* obsoleted by setpgid() */
int	 setregid(int, int);
int	 setreuid(int, int);
int	 setrgid(gid_t);
int	 setruid(uid_t);
void	 setusershell(void);
int	 strtofflags(char **, u_int32_t *, u_int32_t *);
void	 swab(const void *, void *, size_t);
int	 swapon(const char *);
int	 swapctl(int cmd, const void *arg, int misc);
int	 symlink(const char *, const char *);
void	 sync(void);
int	 syscall(int, ...);
int	 truncate(const char *, off_t);
int	 ttyslot(void);
unsigned int	 ualarm(unsigned int, unsigned int);
int	 undelete(const char *);
int	 usleep(useconds_t);
void	*valloc(size_t);		/* obsoleted by malloc() */
pid_t	 vfork(void);
int	 issetugid(void);

int	 getopt(int, char * const *, const char *);
extern	 char *optarg;			/* getopt(3) external variables */
extern	 int opterr;
extern	 int optind;
extern	 int optopt;
extern	 int optreset;
int	 getsubopt(char **, char * const *, char **);
extern	 char *suboptarg;		/* getsubopt(3) external variable */
#endif /* !_POSIX_SOURCE */

#if (!defined(_POSIX_SOURCE) && !defined(_POSIX_C_SOURCE) && \
     !defined(_XOPEN_SOURCE)) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE_EXTENDED - 0 == 1)
#define F_ULOCK         0
#define F_LOCK          1
#define F_TLOCK         2
#define F_TEST          3
int     lockf(int, int, off_t);
#endif /* (!defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)) || ... */
__END_DECLS

#endif /* !_UNISTD_H_ */
