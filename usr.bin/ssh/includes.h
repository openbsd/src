/*

includes.h

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Thu Mar 23 16:29:37 1995 ylo

This file includes most of the needed system headers.

*/

/* RCSID("$Id: includes.h,v 1.3 1999/09/29 18:16:19 dugsong Exp $"); */

#ifndef INCLUDES_H
#define INCLUDES_H

/* Note: autoconf documentation tells to use the <...> syntax and have -I. */
#include <config.h>

#include "version.h"

typedef unsigned short word16;

#if SIZEOF_LONG == 4
typedef unsigned long word32;
#else
#if SIZEOF_INT == 4
typedef unsigned int word32;
#else
#if SIZEOF_SHORT >= 4
typedef unsigned short word32;
#else
YOU_LOSE
#endif
#endif
#endif

#ifdef SCO
/* this is defined so that winsize gets ifdef'd in termio.h */
#define _IBCS2
#endif

#if defined(__mips)
/* Mach3 on MIPS defines conflicting garbage. */
#define uint32 hidden_uint32
#endif /* __mips */
#include <sys/types.h>
#if defined(__mips)
#undef uint32
#endif /* __mips */

#if defined(bsd_44) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__PARAGON__)
#include <sys/param.h>
#include <machine/endian.h>
#include <netgroup.h>
#endif
#if defined(linux)
#include <endian.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#define USING_TERMIOS
#endif /* HAVE_TERMIOS_H */

#if defined(HAVE_SGTTY_H) && !defined(USING_TERMIOS)
#include <sgtty.h>
#define USING_SGTTY
#endif

#if !defined(USING_SGTTY) && !defined(USING_TERMIOS)
  ERROR_NO_TERMIOS_OR_SGTTY
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#else /* STDC_HEADERS */
/* stdarg.h is present almost everywhere, and comes with gcc; I am too lazy
   to make things work with both it and varargs. */
#include <stdarg.h>
#ifndef HAVE_STRCHR
#define strchr index
#define strrchr rindex
#endif
char *strchr(), *strrchr();
#ifndef HAVE_MEMCPY
#define memcpy(d, s, n) bcopy((s), (d), (n))
#define memmove(d, s, n) bcopy((s), (d), (n))
#define memset(d, ch, n) bzero((d), (n)) /* We only memset to 0. */
#define memcmp(a, b, n) bcmp((a), (b), (n))
#endif
#endif /* STDC_HEADERS */

#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#else /* Some old linux systems at least have in_system.h instead. */
#include <netinet/in_system.h>
#endif /* HAVE_NETINET_IN_SYSTM_H */
#ifdef SCO
/* SCO does not have a un.h and there is no appropriate substitute. */
/* Latest news: it doesn't have AF_UNIX at all, but this allows
   it to compile, and outgoing forwarded connections appear to work. */
struct	sockaddr_un {
	short	sun_family;		/* AF_UNIX */
	char	sun_path[108];		/* path name (gag) */
};
/* SCO needs sys/stream.h and sys/ptem.h */
#include <sys/stream.h>
#include <sys/ptem.h>
#else /* SCO */
#include <sys/un.h>
#endif /* SCO */
#if !defined(__PARAGON__)
#include <netinet/ip.h>
#endif /* !__PARAGON__ */
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#include <pwd.h>
#include <grp.h>
#ifdef HAVE_GETSPNAM
#include <shadow.h>
#endif /* HAVE_GETSPNAM */

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#else /* HAVE_SYS_WAIT_H */
#if !defined(WNOHANG) /* && (defined(bsd43) || defined(vax)) */
#define WNOHANG 1
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(X) ((unsigned)(X) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(X) (((X) & 255) == 0)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(X) ((((X) & 255) != 0x255 && ((X) & 255) != 0))
#endif
#ifndef WTERMSIG
#define WTERMSIG(X) ((X) & 255)
#endif
#endif /* HAVE_SYS_WAIT_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef TIME_WITH_SYS_TIME
#ifndef SCO
/* I excluded <sys/time.h> to avoid redefinition of timeval 
   which SCO puts in both <sys/select.h> and <sys/time.h> */
#include <sys/time.h>
#endif /* SCO */
#include <time.h>
#else /* TIME_WITH_SYS_TIME */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else /* HAVE_SYS_TIME_H */
#include <time.h>
#endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH_SYS_TIME */

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#if HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif

#ifdef HAVE_SETRLIMIT
#include <sys/resource.h>
#endif

/* These POSIX macros are not defined in every system. */

#ifndef S_IRWXU
#define S_IRWXU 00700		/* read, write, execute: owner */
#define S_IRUSR 00400		/* read permission: owner */
#define S_IWUSR 00200		/* write permission: owner */
#define S_IXUSR 00100		/* execute permission: owner */
#define S_IRWXG 00070		/* read, write, execute: group */
#define S_IRGRP 00040		/* read permission: group */
#define S_IWGRP 00020		/* write permission: group */
#define S_IXGRP 00010		/* execute permission: group */
#define S_IRWXO 00007		/* read, write, execute: other */
#define S_IROTH 00004		/* read permission: other */
#define S_IWOTH 00002		/* write permission: other */
#define S_IXOTH 00001		/* execute permission: other */
#endif /* S_IRWXU */

#ifndef S_ISUID
#define S_ISUID 0x800
#endif /* S_ISUID */
#ifndef S_ISGID
#define S_ISGID 0x400
#endif /* S_ISGID */

#ifndef S_ISDIR
/* NextStep apparently fails to define this. */
#define S_ISDIR(mode)   (((mode)&(_S_IFMT))==(_S_IFDIR))
#endif

#ifdef STAT_MACROS_BROKEN
/* Some systems have broken S_ISDIR etc. macros in sys/stat.h.  Please ask
   your vendor to fix them.  You can then remove the line below, but only
   after you have sent a complaint to your vendor. */
WARNING_MACROS_IN_SYS_STAT_H_ARE_BROKEN_ON_YOUR_SYSTEM_READ_INCLUDES_H
#endif /* STAT_MACROS_BROKEN */

#if USE_STRLEN_FOR_AF_UNIX
#define AF_UNIX_SIZE(unaddr) \
  (sizeof((unaddr).sun_family) + strlen((unaddr).sun_path) + 1)
#else
#define AF_UNIX_SIZE(unaddr) sizeof(unaddr)
#endif

#endif /* INCLUDES_H */
