/*	$OpenBSD: roken.h,v 1.1.1.1 1998/09/14 21:53:07 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $KTH: roken.h,v 1.9 1998/06/07 05:19:20 map Exp $ */

#ifndef __ROKEN_H__
#define __ROKEN_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_NETINET6_IN6_H
#include <netinet6/in6.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#if defined(HAVE_SYS_IOCTL_H) && SunOS != 4
#include <sys/ioctl.h>
#endif

#if !defined(HAVE_SETSID) && defined(HAVE__SETSID)
#define setsid _setsid
#endif

#ifndef HAVE_PUTENV
int putenv(const char *string);
#endif

#ifndef HAVE_SETENV
int setenv(const char *var, const char *val, int rewrite);
#endif

#ifndef HAVE_UNSETENV
void unsetenv(const char *name);
#endif

#if !defined(HAVE_GETUSERSHELL) || defined(NEED_GETUSERSHELL_PROTO)
char *getusershell(void);
#endif

#ifndef HAVE___ATTRIBUTE__
#define __attribute__(x)
#endif

#ifndef HAVE_SNPRINTF
int snprintf (char *str, size_t sz, const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));
#endif

#ifndef HAVE_VSNPRINTF
int vsnprintf (char *str, size_t sz, const char *format, va_list ap)
     __attribute__((format (printf, 3, 0)));
#endif

#ifndef HAVE_ASPRINTF
int asprintf (char **ret, const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));
#endif

#ifndef HAVE_VASPRINTF
int vasprintf (char **ret, const char *format, va_list ap)
     __attribute__((format (printf, 2, 0)));
#endif

#ifndef HAVE_ASNPRINTF
int asnprintf (char **ret, size_t max_sz, const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));
#endif

#ifndef HAVE_VASNPRINTF
int vasnprintf (char **ret, size_t max_sz, const char *format, va_list ap)
     __attribute__((format (printf, 3, 0)));
#endif

#ifndef HAVE_STRDUP
char * strdup(const char *old);
#endif

#ifndef HAVE_STRLWR
char * strlwr(char *);
#endif

#ifndef HAVE_STRNLEN
int strnlen(char*, int);
#endif

#ifndef HAVE_STRSEP
char *strsep(char**, const char*);
#endif

#ifdef NEED_FCLOSE_PROTO
int fclose(FILE *);
#endif

#ifdef NEED_STRTOK_R_PROTO
char *strtok_r(char *s1, const char *s2, char **lasts);
#endif

#ifndef HAVE_STRUPR
char * strupr(char *);
#endif

#ifndef HAVE_GETDTABLESIZE
int getdtablesize(void);
#endif

#if IRIX != 4 /* fix for compiler bug */
#ifdef RETSIGTYPE
typedef RETSIGTYPE (*SigAction)(/* int??? */);
SigAction signal(int iSig, SigAction pAction); /* BSD compatible */
#endif
#endif

#ifndef SIG_ERR
#define SIG_ERR ((RETSIGTYPE (*)())-1)
#endif

#if !defined(HAVE_STRERROR) && !defined(strerror)
char *strerror(int eno);
#endif

#ifdef NEED_HSTRERROR_PROTO
#ifdef NEED_HSTRERROR_CONST
const
#endif
char *hstrerror(int herr);
#endif

#ifndef HAVE_H_ERRNO_DECLARATION
extern int h_errno;
#endif

#ifndef HAVE_INET_ATON
/* Minimal implementation of inet_aton. Doesn't handle hex numbers. */
int inet_aton(const char *cp, struct in_addr *adr);
#endif

#if !defined(HAVE_GETCWD)
char* getcwd(char *path, size_t size);
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
struct passwd *k_getpwnam (char *user);
struct passwd *k_getpwuid (uid_t uid);
#endif

#ifndef HAVE_SETEUID
int seteuid(uid_t euid);
#endif

#ifndef HAVE_SETEGID
int setegid(gid_t egid);
#endif

#ifndef HAVE_LSTAT
int lstat(const char *path, struct stat *buf);
#endif

#ifndef HAVE_MKSTEMP
int mkstemp(char *);
#endif

#ifndef HAVE_INITGROUPS
int initgroups(const char *name, gid_t basegid);
#endif

#ifndef HAVE_FCHOWN
int fchown(int fd, uid_t owner, gid_t group);
#endif

#ifndef HAVE_CHOWN
int chown(const char *path, uid_t owner, gid_t group);
#endif

#ifndef HAVE_RCMD
int rcmd(char **ahost, unsigned short inport, const char *locuser,
	 const char *remuser, const char *cmd, int *fd2p);
#endif

#ifndef HAVE_WRITEV
ssize_t
writev(int d, const struct iovec *iov, int iovcnt);
#endif

#ifndef HAVE_READV
ssize_t
readv(int d, const struct iovec *iov, int iovcnt);
#endif

#ifndef HAVE_FLOCK
#ifndef LOCK_SH
#define LOCK_SH   1		/* Shared lock */
#endif
#ifndef	LOCK_EX
#define LOCK_EX   2		/* Exclusive lock */
#endif
#ifndef LOCK_NB
#define LOCK_NB   4		/* Don't block when locking */
#endif
#ifndef LOCK_UN
#define LOCK_UN   8		/* Unlock */
#endif

int flock(int fd, int operation);
#endif /* HAVE_FLOCK */

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif

time_t tm2time (struct tm tm, int local);

int unix_verify_user(char *user, char *password);

void inaddr2str(struct in_addr addr, char *s, size_t len);

void mini_inetd (int port);

#ifndef HAVE_STRUCT_WINSIZE
struct winsize {
	unsigned short ws_row, ws_col;
	unsigned short ws_xpixel, ws_ypixel;
};
#endif

int get_window_size(int fd, struct winsize *);

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#ifndef SOMAXCONN
#define SOMAXCONN 5
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
/* Misc definitions for old syslogs */

#ifndef LOG_DAEMON
#define openlog(id,option,facility) openlog((id),(option))
#define	LOG_DAEMON	0
#endif
#ifndef LOG_ODELAY
#define LOG_ODELAY 0
#endif
#ifndef LOG_NDELAY
#define LOG_NDELAY 0x08
#endif
#ifndef LOG_CONS
#define LOG_CONS 0
#endif
#ifndef LOG_AUTH
#define LOG_AUTH 0
#endif
#ifndef LOG_AUTHPRIV
#define LOG_AUTHPRIV LOG_AUTH
#endif
#endif

#ifndef HAVE_VSYSLOG
void vsyslog(int pri, const char *fmt, va_list ap);
#endif

#ifndef HAVE_OPTARG_DECLARATION
extern char *optarg;
#endif
#ifndef HAVE_OPTIND_DECLARATION
extern int optind;
#endif
#ifndef HAVE_OPTERR_DECLARATION
extern int opterr;
#endif

#ifndef HAVE___PROGNAME_DECLARATION
extern const char *__progname;
#endif

/*
 * kludges and such
 */

#ifdef GETHOSTBYNAME_PROTO_COMPATIBLE
#define roken_gethostbyname(x) gethostbyname(x)
#else
#define roken_gethostbyname(x) gethostbyname((char *)x)
#endif

#ifdef GETHOSTBYADDR_PROTO_COMPATIBLE
#define roken_gethostbyaddr(a, l, t) gethostbyaddr(a, l, t)
#else
#define roken_gethostbyaddr(a, l, t) gethostbyaddr((char *)a, l, t)
#endif

#ifdef GETSERVBYNAME_PROTO_COMPATIBLE
#define roken_getservbyname(x,y) getservbyname(x,y)
#else
#define roken_getservbyname(x,y) getservbyname((char *)x, (char *)y)
#endif

#ifdef OPENLOG_PROTO_COMPATIBLE
#define roken_openlog(a,b,c) openlog(a,b,c)
#else
#define roken_openlog(a,b,c) openlog((char *)a,b,c)
#endif

void set_progname(char *argv0);

#ifndef F_OK
#define F_OK 0
#endif

#ifndef O_ACCMODE
#define O_ACCMODE	003
#endif

#ifndef O_BINARY
#define O_BINARY	0
#endif

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN (1024+4)
#endif


#ifdef NEED_SELECT_PROTO
int
select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
       struct timeval *timeout);
#endif

#ifndef HAVE_SSIZE_T
typedef int ssize_t;  /* XXX real hot stuff */
#endif

#ifdef HAVE_REPAIRABLE_HTONL
#define htonl(x) __cpu_to_be32(x)
#define ntohl(x) __be32_to_cpu(x)
#define htons(x) __cpu_to_be16(x)
#define ntohs(x) __be16_to_cpu(x)
#endif

#endif /*  __ROKEN_H__ */
