/*
 * Copyright (c) 1996, 1998-2003 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Sudo: compat.h,v 1.65 2003/03/15 20:31:02 millert Exp $
 */

#ifndef _SUDO_COMPAT_H
#define _SUDO_COMPAT_H

/*
 * Macros that may be missing on some Operating Systems
 */

/* Deal with ANSI stuff reasonably.  */
#ifndef  __P
# if defined (__cplusplus) || defined (__STDC__)
#  define __P(args)		args
# else
#  define __P(args)		()
# endif
#endif /* __P */

/*
 * Some systems (ie ISC V/386) do not define MAXPATHLEN even in param.h
 */
#ifndef MAXPATHLEN
# define MAXPATHLEN		1024
#endif

/*
 * Some systems do not define MAXHOSTNAMELEN.
 */
#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN		64
#endif

/*
 * 4.2BSD lacks FD_* macros (we only use FD_SET and FD_ZERO)
 */
#ifndef FD_SETSIZE
# define FD_SET(fd, fds)	((fds) -> fds_bits[0] |= (1 << (fd)))
# define FD_ZERO(fds)		((fds) -> fds_bits[0] = 0)
#endif /* !FD_SETSIZE */

/*
 * Posix versions for those without...
 */
#ifndef _S_IFMT
# define _S_IFMT		S_IFMT
#endif /* _S_IFMT */
#ifndef _S_IFREG
# define _S_IFREG		S_IFREG
#endif /* _S_IFREG */
#ifndef _S_IFDIR
# define _S_IFDIR		S_IFDIR
#endif /* _S_IFDIR */
#ifndef _S_IFLNK
# define _S_IFLNK		S_IFLNK
#endif /* _S_IFLNK */
#ifndef S_ISREG
# define S_ISREG(m)		(((m) & _S_IFMT) == _S_IFREG)
#endif /* S_ISREG */
#ifndef S_ISDIR
# define S_ISDIR(m)		(((m) & _S_IFMT) == _S_IFDIR)
#endif /* S_ISDIR */

/*
 * Some OS's may not have this.
 */
#ifndef S_IRWXU
# define S_IRWXU		0000700		/* rwx for owner */
#endif /* S_IRWXU */

/*
 * In case this is not defined in <sys/types.h> or <sys/select.h>
 */
#ifndef howmany
# define howmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif

/*
 * These should be defined in <unistd.h> but not everyone has them.
 */
#ifndef STDIN_FILENO
# define	STDIN_FILENO	0
#endif
#ifndef STDOUT_FILENO
# define	STDOUT_FILENO	1
#endif
#ifndef STDERR_FILENO
# define	STDERR_FILENO	2
#endif

/*
 * These should be defined in <unistd.h> but not everyone has them.
 */
#ifndef SEEK_SET
# define	SEEK_SET	0
#endif
#ifndef SEEK_CUR
# define	SEEK_CUR	1
#endif
#ifndef SEEK_END
# define	SEEK_END	2
#endif

/*
 * BSD defines these in <sys/param.h> but others may not.
 */
#ifndef MIN
# define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
# define MAX(a,b) (((a)>(b))?(a):(b))
#endif

/*
 * Simple isblank() macro for systems without it.
 */
#ifndef HAVE_ISBLANK
# define isblank(_x)	((_x) == ' ' || (_x) == '\t')
#endif

/*
 * Old BSD systems lack strchr(), strrchr(), memset() and memcpy()
 */
#if !defined(HAVE_STRCHR) && !defined(strchr)
# define strchr(_s, _c)	index(_s, _c)
#endif
#if !defined(HAVE_STRRCHR) && !defined(strrchr)
# define strrchr(_s, _c)	rindex(_s, _c)
#endif
#if !defined(HAVE_MEMCPY) && !defined(memcpy)
# define memcpy(_d, _s, _n)	(bcopy(_s, _d, _n))
#endif
#if !defined(HAVE_MEMSET) && !defined(memset)
# define memset(_s, _x, _n)	(bzero(_s, _n))
#endif

/*
 * NCR's SVr4 has _innetgr(3) instead of innetgr(3) for some reason.
 */
#ifdef HAVE__INNETGR
# define innetgr(n, h, u, d)	(_innetgr(n, h, u, d))
# define HAVE_INNETGR 1
#endif /* HAVE__INNETGR */

/*
 * On POSIX systems, O_NOCTTY is the default so some OS's may lack this define.
 */
#ifndef O_NOCTTY
# define O_NOCTTY	0
#endif /* O_NOCTTY */

/*
 * Emulate POSIX signals via sigvec(2)
 */
#ifndef HAVE_SIGACTION
# define SA_ONSTACK	SV_ONSTACK
# define SA_RESTART	SV_INTERRUPT		/* opposite effect */
# define SA_RESETHAND	SV_RESETHAND
# define sa_handler	sv_handler
# define sa_mask	sv_mask
# define sa_flags	sv_flags
typedef struct sigvec sigaction_t;
typedef int sigset_t;
int sigaction __P((int sig, const sigaction_t *act, sigaction_t *oact));
int sigemptyset __P((sigset_t *));
int sigfillset __P((sigset_t *));
int sigaddset __P((sigset_t *, int));
int sigdelset __P((sigset_t *, int));
int sigismember __P((sigset_t *, int));
int sigprocmask __P((int, const sigset_t *, sigset_t *));
#endif

/*
 * Extra sugar for POSIX signals to deal with the above emulation
 * as well as the fact that SunOS has a SA_INTERRUPT flag.
 */
#ifdef HAVE_SIGACTION
# ifndef HAVE_SIGACTION_T
typedef struct sigaction sigaction_t;
# endif
# ifndef SA_INTERRUPT 
#  define SA_INTERRUPT	0
# endif
# ifndef SA_RESTART 
#  define SA_RESTART	0
# endif
#endif

/*
 * HP-UX 9.x has RLIMIT_* but no RLIM_INFINITY.
 * Using -1 works because we only check for RLIM_INFINITY and do not set it.
 */
#ifndef RLIM_INFINITY
# define RLIM_INFINITY	(-1)
#endif

#endif /* _SUDO_COMPAT_H */
