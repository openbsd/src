/*	$OpenBSD: config.h,v 1.5 1998/09/15 02:42:43 millert Exp $	*/

/* config.h.  Generated automatically by configure.  */
/*
 *  CU sudo version 1.5.6
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *  $From: config.h.in,v 1.95 1998/09/11 23:23:33 millert Exp $
 */

/*
 * config.h -- You shouldn't edit this by hand unless you are
 *             NOT using configure.
 */

/* New ANSI-style OS defs.  */
#if defined(hpux) && !defined(__hpux)
#  define __hpux	1
#endif /* hpux */

#if defined(convex) && !defined(__convex__)
#  define __convex__	1
#endif /* convex */

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if on ConvexOs.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _CONVEX_SOURCE
/* #undef _CONVEX_SOURCE */
#endif

/* Define if needed to get POSIX functionality.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _POSIX_SOURCE
/* #undef _POSIX_SOURCE */
#endif

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef ssize_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef dev_t */

/* Define to `unsigned int' if <sys/types.h> doesn't define.  */
/* #undef ino_t */

/* Define to be nil if C compiler doesn't support "const."  */
/* #undef const */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you want to use the system getpass().  */
/* #undef USE_GETPASS */

/* Define if you use S/Key.  */
/* #undef HAVE_SKEY */

/* Define if you use NRL OPIE.  */
/* #undef HAVE_OPIE */

/* Define if you use SecurID.  */
/* #undef HAVE_SECURID */

/* Define if you use AIX general authentication.  */
/* #undef HAVE_AUTHENTICATE */

/* Define if you use Kerberos.  */
/* #undef HAVE_KERB4 */

/* Define if you use Kerberos.  */
/* #undef HAVE_KERB5 */

/* Keberos v5 has v4 compatibility */
#ifdef HAVE_KERB5
#  define HAVE_KERB4
#endif /* HAVE_KERB5 */

/* Define if you use PAM.  */
/* #undef HAVE_PAM */

/* Define if you use AFS.  */
/* #undef HAVE_AFS */

/* Define if you use OSF DCE.  */
/* #undef HAVE_DCE */

/* Define if you have POSIX signals.  */
#define HAVE_SIGACTION 1
#ifdef HAVE_SIGACTION
#  define POSIX_SIGNALS
#endif /* HAVE_SIGACTION */

/* Define if you have tzset(3).  */
#define HAVE_TZSET 1

/* Define if you have getcwd(3).  */
#define HAVE_GETCWD 1

/* Define if you have strdup(3).  */
#define HAVE_STRDUP 1

/* Define if you have fnmatch(3).  */
#define HAVE_FNMATCH 1

/* Define if you have lsearch(3).  */
#define HAVE_LSEARCH 1

/* Define if you have strchr(3).  */
#define HAVE_STRCHR 1
#if !defined(HAVE_STRCHR) && !defined(strchr)
#  define strchr	index
#endif

/* Define if you have strrchr(3).  */
#define HAVE_STRRCHR 1
#if !defined(HAVE_STRRCHR) && !defined(strrchr)
#  define strrchr	rindex
#endif

/* Define if you have memcpy(3).  */
#define HAVE_MEMCPY 1
#if !defined(HAVE_MEMCPY) && !defined(memcpy)
#  define memcpy(D, S, L)	(bcopy(S, D, L))
#endif

/* Define if you have memset(3).  */
#define HAVE_MEMSET 1
#if !defined(HAVE_MEMSET) && !defined(memset)
#  define memset(S, X, N)	(bzero(S, N))
#endif

/* Define if you have sysconf(3c). */
#define HAVE_SYSCONF 1

/* Define if you have putenv(3). */
/* #undef HAVE_PUTENV */

/* Define if you have setenv(3). */
#define HAVE_SETENV 1

/* Define if you have strcasecmp(3). */
#define HAVE_STRCASECMP 1

/* Define if you have tcgetattr(3). */
#define HAVE_TCGETATTR 1

/* Define if you have innetgr(3). */
#define HAVE_INNETGR 1

/* Define if you have getdomainname(2). */
#define HAVE_GETDOMAINNAME 1

/* Define if you have utime(2). */
#define HAVE_UTIME 1

/* Define if you have a POSIX utime() (uses struct utimbuf) */
#define HAVE_UTIME_POSIX 1

/* Define if utime(file, NULL) sets timestamp to current */
#define HAVE_UTIME_NULL 1

/* Define if you have bigcrypt(3). */
/* #undef HAVE_BIGCRYPT */

/* Define if you have set_auth_parameters(3). */
/* #undef HAVE_SET_AUTH_PARAMETERS */

/* Define if you have seteuid(3). */
#define HAVE_SETEUID 1

/* Define if you have waitpid(2). */
#define HAVE_WAITPID 1

/* Define if you have wait3(2). */
/* #undef HAVE_WAIT3 */

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <alloca.h> header file.  */
/* #undef HAVE_ALLOCA_H */

/* Define if you have the <paths.h> header file.  */
#define HAVE_PATHS_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#if !defined(__convex__) && !defined(convex)
#define HAVE_STRINGS_H 1
#endif /* convex */

/* Define your flavor of dir entry header file.  */
#define HAVE_DIRENT_H 1
/* #undef HAVE_SYS_NDIR_H */
/* #undef HAVE_SYS_DIR_H */
/* #undef HAVE_NDIR_H */

/* Define if you have the <utime.h> header file.  */
#define HAVE_UTIME_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <fnmatch.h> header file.  */
#define HAVE_FNMATCH_H 1

/* Define if you have the <netgroup.h> header file.  */
#define HAVE_NETGROUP_H 1

/* Define if you have the <termio.h> header file.  */
/* #undef HAVE_TERMIO_H */

/* Define if you have the <termios.h> header file and tcgetattr(3).  */
#ifdef HAVE_TCGETATTR
#define HAVE_TERMIOS_H 1
#endif /* HAVE_TCGETATTR */

/* Define if you have the <sys/sockio.h> header file.  */
#define HAVE_SYS_SOCKIO_H 1

/* Define if you have the <sys/bsdtypes.h> header file.  */
/* #undef HAVE_SYS_BSDTYPES_H */

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if your struct sockadr has an sa_len field. */
#define HAVE_SA_LEN 1

/* Supported shadow password types */
#define SPW_NONE		0x00
#define SPW_SECUREWARE		0x01
#define SPW_HPUX9		0x02
#define SPW_SUNOS4		0x03
#define SPW_SVR4		0x04
#define SPW_ULTRIX4		0x05
#define SPW_BSD			0x06

/* Define to the variety of shadow passwords supported on your OS */
#define SHADOW_TYPE SPW_BSD

/* Define to void if your C compiler fully groks void, else char */
#define VOID void

/* Define to the max length of a uid_t in string context (excluding the NULL */
#define MAX_UID_T_LEN 10

/* Define if your syslog(3) does not guarantee the message will be logged */
/* and syslog(3) returns non-zero to denote failure */
/* #undef BROKEN_SYSLOG */

/*
 * Emulate a subset of waitpid() if we don't have it.
 */
#ifdef HAVE_WAITPID
#define sudo_waitpid(p, s, o)	waitpid(p, s, o)
#else
#ifdef HAVE_WAIT3
#define sudo_waitpid(p, s, o)	wait3(s, o, NULL)
#endif
#endif

/* Define if you want the hostname to be entered into the log file */
/* #undef HOST_IN_LOG */

/* Define if you want the log file line to be wrapped */
#define WRAP_LOG 1

/*
 * Paths to commands used by sudo.  There are used by pathnames.h.
 * If you want to override these values, do so in pathnames.h, not here!
 */

#ifndef _CONFIG_PATH_SENDMAIL  
#define _CONFIG_PATH_SENDMAIL "/usr/sbin/sendmail"
#endif /* _CONFIG_PATH_SENDMAIL */

#ifndef _CONFIG_PATH_VI
#define _CONFIG_PATH_VI "/usr/bin/vi"
#endif /* _CONFIG_PATH_VI */
  
#ifndef _CONFIG_PATH_PWD
#define _CONFIG_PATH_PWD "/bin/pwd"
#endif /* _CONFIG_PATH_PWD */

#ifndef _CONFIG_PATH_MV
#define _CONFIG_PATH_MV "/bin/mv"
#endif /* _CONFIG_PATH_MV */

#ifndef _CONFIG_PATH_BSHELL
#define _CONFIG_PATH_BSHELL "/bin/sh"
#endif /* _CONFIG_PATH_BSHELL */

#ifndef _CONFIG_PATH_LOGFILE
#define _CONFIG_PATH_LOGFILE "/var/log/sudo.log"
#endif /* _CONFIG_PATH_LOGFILE */

#ifndef _CONFIG_PATH_TIMEDIR
#define _CONFIG_PATH_TIMEDIR "/var/run/sudo"
#endif /* _CONFIG_PATH_TIMEDIR */
