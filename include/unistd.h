/*	$OpenBSD: unistd.h,v 1.24 1998/11/20 11:18:26 d Exp $ */
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
#define	NULL		0	/* null pointer constant */
#endif

__BEGIN_DECLS
__dead void	 _exit __P((int)) __attribute__((noreturn));
int	 access __P((const char *, int));
unsigned int alarm __P((unsigned int));
int	 chdir __P((const char *));
int	 chown __P((const char *, uid_t, gid_t));
int	 close __P((int));
size_t	 confstr __P((int, char *, size_t));
char	*cuserid __P((char *));
int	 dup __P((int));
int	 dup2 __P((int, int));
int	 execl __P((const char *, const char *, ...));
int	 execle __P((const char *, const char *, ...));
int	 execlp __P((const char *, const char *, ...));
int	 execv __P((const char *, char * const *));
int	 execve __P((const char *, char * const *, char * const *));
int	 execvp __P((const char *, char * const *));
pid_t	 fork __P((void));
long	 fpathconf __P((int, int));
char	*getcwd __P((char *, size_t));
gid_t	 getegid __P((void));
uid_t	 geteuid __P((void));
gid_t	 getgid __P((void));
int	 getgroups __P((int, gid_t *));
char	*getlogin __P((void));
int	 getlogin_r __P((char *, size_t));
pid_t	 getpgrp __P((void));
pid_t	 getpid __P((void));
pid_t	 getpgid __P((pid_t));
pid_t	 getppid __P((void));
uid_t	 getuid __P((void));
int	 isatty __P((int));
int	 link __P((const char *, const char *));
off_t	 lseek __P((int, off_t, int));
long	 pathconf __P((const char *, int));
int	 pause __P((void));
int	 pipe __P((int *));
ssize_t	 read __P((int, void *, size_t));
int	 rmdir __P((const char *));
int	 setgid __P((gid_t));
int	 setpgid __P((pid_t, pid_t));
pid_t	 setsid __P((void));
int	 setuid __P((uid_t));
unsigned int sleep __P((unsigned int));
long	 sysconf __P((int));
pid_t	 tcgetpgrp __P((int));
int	 tcsetpgrp __P((int, pid_t));
char	*ttyname __P((int));
int	 ttyname_r __P((int, char *, size_t));
int	 unlink __P((const char *));
ssize_t	 write __P((int, const void *, size_t));

#ifndef	_POSIX_SOURCE

/* structure timeval required for select() */
#include <sys/time.h>

int	 acct __P((const char *));
char	*brk __P((const char *));
int	 chroot __P((const char *));
char	*crypt __P((const char *, const char *));
int	 des_cipher __P((const char *, char *, long, int));
int	 des_setkey __P((const char *key));
int	 encrypt __P((char *, int));
void	 endusershell __P((void));
int	 exect __P((const char *, char * const *, char * const *));
int	 fchdir __P((int));
int	 fchown __P((int, uid_t, gid_t));
int	 fsync __P((int));
int	 ftruncate __P((int, off_t));
int	 getdomainname __P((char *, size_t));
int	 getdtablesize __P((void));
int	 getgrouplist __P((const char *, gid_t, gid_t *, int *));
long	 gethostid __P((void));
int	 gethostname __P((char *, size_t));
mode_t	 getmode __P((const void *, mode_t));
int	 getpagesize __P((void));
char	*getpass __P((const char *));
char	*getusershell __P((void));
char	*getwd __P((char *));			/* obsoleted by getcwd() */
int	 initgroups __P((const char *, gid_t));
int	 iruserok __P((u_int32_t, int, const char *, const char *));
int	 lchown __P((const char *, uid_t, gid_t));
char	*mkdtemp __P((char *));
int	 mkstemp __P((char *));
int	 mkstemps __P((char *, int));
char	*mktemp __P((char *));
int	 nfssvc __P((int, void *));
int	 nice __P((int));
void	 psignal __P((unsigned int, const char *));
extern __const char *__const sys_siglist[];
int	 profil __P((char *, size_t, unsigned long, unsigned int));
int	 rcmd __P((char **, int, const char *,
		const char *, const char *, int *));
int	 rcmdsh __P((char **, int, const char *,
		const char *, const char *, char *));
char	*re_comp __P((const char *));
int	 re_exec __P((const char *));
int	 readlink __P((const char *, char *, size_t));
int	 reboot __P((int));
int	 revoke __P((const char *));
int	 rfork __P((int opts));
int	 rresvport __P((int *));
int	 ruserok __P((const char *, int, const char *, const char *));
int	 quotactl __P((const char *, int, int, char *));
char	*sbrk __P((int));
#ifndef _XOPEN_SOURCE
int	 select __P((int, fd_set *, fd_set *, fd_set *, struct timeval *));
#endif
int	 setdomainname __P((const char *, size_t));
int	 setegid __P((gid_t));
int	 seteuid __P((uid_t));
int	 setgroups __P((int, const gid_t *));
int	 sethostid __P((long));
int	 sethostname __P((const char *, size_t));
int	 setkey __P((const char *));
int	 setlogin __P((const char *));
void	*setmode __P((const char *));
int	 setpgrp __P((pid_t pid, pid_t pgrp));	/* obsoleted by setpgid() */
int	 setregid __P((int, int));
int	 setreuid __P((int, int));
int	 setrgid __P((gid_t));
int	 setruid __P((uid_t));
void	 setusershell __P((void));
void	 swab __P((const void *, void *, size_t));
int	 swapon __P((const char *));
int	 symlink __P((const char *, const char *));
void	 sync __P((void));
int	 syscall __P((int, ...));
int	 truncate __P((const char *, off_t));
int	 ttyslot __P((void));
unsigned int	 ualarm __P((unsigned int, unsigned int));
int	 undelete __P((const char *));
int	 usleep __P((useconds_t));
void	*valloc __P((size_t));			/* obsoleted by malloc() */
pid_t	 vfork __P((void));
int	 issetugid __P((void));

int	 getopt __P((int, char * const *, const char *));
extern	 char *optarg;			/* getopt(3) external variables */
extern	 int opterr;
extern	 int optind;
extern	 int optopt;
extern	 int optreset;
int	 getsubopt __P((char **, char * const *, char **));
extern	 char *suboptarg;		/* getsubopt(3) external variable */
#endif /* !_POSIX_SOURCE */

#if (!defined(_POSIX_SOURCE) && !defined(_POSIX_C_SOURCE) && \
     !defined(_XOPEN_SOURCE)) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE_EXTENDED - 0 == 1)
#define F_ULOCK         0
#define F_LOCK          1
#define F_TLOCK         2
#define F_TEST          3
int     lockf __P((int, int, off_t));
#endif /* (!defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)) || ... */
__END_DECLS

#endif /* !_UNISTD_H_ */
