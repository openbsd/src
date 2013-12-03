/* config.h --- configuration file for OS/2 EMX
   Thomas Epting <tepting@swol.de> --- Feb 1997  */

/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* This file lives in the emx/ subdirectory, which is only included
 * in your header search path if you use emx/Makefile (with GNU make
 * for OS/2). Thus, this is the right place to put configuration
 * information for OS/2.
 */

#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define to empty if the keyword does not work.  */
#undef const

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if you support file names longer than 14 characters.  */
#define HAVE_LONG_FILE_NAMES 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if utime(file, NULL) sets file's timestamp to the present.  */
#define HAVE_UTIME_NULL 1

/* Define if on MINIX.  */
/* #undef _MINIX */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define if you have MIT Kerberos version 4 available.  */
#undef HAVE_KERBEROS

/* Define if you want CVS to be able to be a remote repository client.  */
#define CLIENT_SUPPORT 1

#if 0
/* This doesn't work yet, and I'm sure I don't want the hassles of seeing
   whether it will compile.  */
/* Define if you want CVS to be able to serve repositories to remote
   clients.  */
#define SERVER_SUPPORT 1

/* Define if you want to use the password authenticated server.  */
#define AUTH_SERVER_SUPPORT
#endif /* 0 */

/* Define if you want encryption support.  */
/* #undef ENCRYPTION */

/* Define if you have the connect function.  */
#define HAVE_CONNECT 1

/* Define if you have the crypt function.  */
#define HAVE_CRYPT 1

/* Define if you have the fchdir function.  */
/* #define HAVE_FCHDIR */

/* Define if you have the fchmod function.  */
/* #define HAVE_FCHMOD */

/* Define if you have the fsync function.  */
#define HAVE_FSYNC 1

/* Define if you have the ftime function.  */
#define HAVE_FTIME 1

/* Define if you have the ftruncate function.  */
#define HAVE_FTRUNCATE 1

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the getspnam function.  */
/* #define HAVE_GETSPNAM */

/* Define if you have the initgroups function.  */
/* #define HAVE_INITGROUPS */

/* Define if you have the krb_get_err_text function.  */
/* #undef HAVE_KRB_GET_ERR_TEXT */

/* Define if you have the putenv function.  */
#define HAVE_PUTENV 1

/* Define if you have the readlink function.  */
/* #define HAVE_READLINK */

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the sigblock function.  */
/* #undef HAVE_SIGBLOCK */

/* Define if you have the sigprocmask function.  */
#define HAVE_SIGPROCMASK 1

/* Define if you have the sigsetmask function.  */
/* #undef HAVE_SIGSETMASK */

/* Define if you have the sigvec function.  */
/* #undef HAVE_SIGVEC */

/* Define if you have the timezone function.  */
#define HAVE_TIMEZONE 1

/* Define if you have the tzset function.  */
#define HAVE_TZSET 1

/* Define if you have the vfork function.  */
/* #undef HAVE_VFORK */

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the <direct.h> header file.  */
/* #undef HAVE_DIRECT_H */

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <io.h> header file.  */
#define HAVE_IO_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <ndbm.h> header file.  */
/* #undef HAVE_NDBM_H 1 */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/bsdtypes.h> header file.  */
/* #undef HAVE_SYS_BSDTYPES_H */

/* Define if you have the <sys/dir.h> header file.  */
#define HAVE_SYS_DIR_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <utime.h> header file.  */
#define HAVE_UTIME_H 1

/* Define if you have the crypt library (-lcrypt).  */
#define HAVE_LIBCRYPT 1

/* Define if you have the inet library (-linet).  */
/* #undef HAVE_LIBINET */

/* Define if you have the nsl library (-lnsl).  */
#define HAVE_LIBNSL 1

/* Define if you have the nsl_s library (-lnsl_s).  */
/* #undef HAVE_LIBNSL_S */

/* Define if you have the sec library (-lsec).  */
/* #undef HAVE_LIBSEC 1 */

/* Define if you have the socket library (-lsocket).  */
#define HAVE_LIBSOCKET 1

/* Under OS/2, filenames are case-insensitive, and both / and \
   are path component separators.  */
#define FOLD_FN_CHAR(c) (OS2_filename_classes[(unsigned char) (c)])
extern unsigned char OS2_filename_classes[];

/* Is the character C a path name separator?  Under OS/2, you can use
   either / or \.  */
#define ISDIRSEP(c) (FOLD_FN_CHAR(c) == '/')

/* Like strcmp, but with the appropriate tweaks for file names.
   Under OS/2, filenames are case-insensitive but case-preserving,
   and both \ and / are path element separators.  */
extern int fncmp (const char *n1, const char *n2);

/* Fold characters in FILENAME to their canonical forms.
   If FOLD_FN_CHAR is not #defined, the system provides a default
   definition for this.  */
extern void fnfold (char *FILENAME);

/* This is where old bits go to die under OS/2 as well as WinNT.  */
#define DEVNULL "nul"

/* We actually do have a transparent rsh, whew. */
#undef RSH_NOT_TRANSPARENT
/* But it won't be transparent unless we ask it nicely! */
#define RSH_NEEDS_BINARY_FLAG 1

#define START_SERVER os2_start_server
#define SHUTDOWN_SERVER os2_shutdown_server

extern void START_SERVER (int *tofd, int *fromfd,
			  char *client_user,
			  char *server_user,
			  char *server_host,
			  char *server_cvsroot);
extern void SHUTDOWN_SERVER (int fd);

/* Call our own os2_initialize function */
#define SYSTEM_INITIALIZE(pargc,pargv) os2_initialize (pargc, pargv)
extern void os2_initialize (int *pargc, char ***pargv);

/* Under EMX, we already have popen() and pclose()... */
/* #undef USE_OWN_POPEN */
/* ... and we too have no need for popenRW to start the rsh server. */
/* #define START_RSH_WITH_POPEN_RW */

/* Socket handles and file handles share a command handle space under EMX. */
/* #undef NO_SOCKET_TO_FD */



#define CVS_STAT     os2_stat
#define CVS_CHDIR    os2_chdir
#define CVS_FNMATCH  os2_fnmatch

extern int os2_stat(const char *name, struct stat *buffer);
extern int os2_chdir(const char *name);
extern int os2_fnmatch(const char *pattern, const char *name, int flags);

/* Pipes need to be put into binary mode using setmode ().  */
#define USE_SETMODE_BINARY 1

/* The reason for this is that we don't know whether to pass -b to
   rsh.  The system-supplied rsh on OS/2 wants it.  Some other rsh
   replacement might not accept it.  Historically, the NT port of CVS
   has not passed -b, and the OS/2 port has.  What a mess.  If we can
   get away with just not accepting :ext: until we can figure out how
   we should deal with this, then it will avoid having people rely on
   behaviors which will need to change.  */
#define NO_EXT_METHOD 1

/* See above; we can't use rsh without -b.  */
#define RSH_NOT_TRANSPARENT 1

/* See discussion at xchmod in filesubr.c.  */
#define CHMOD_BROKEN 1
