/* config.h --- configuration file for OS/2
   Karl Fogel <kfogel@cyclic.com> --- Oct 1995  */

/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* This file lives in the os2/ subdirectory, which is only included
 * in your header search path if you're working under IBM C++,
 * and use os2/makefile (with GNU make for OS/2).  Thus, this is the
 * right place to put configuration information for OS/2.
 */


/* We need some system header files here since we evaluate values from
 * these files below.
 */
#include <stdio.h>
#include <errno.h>



#ifndef __STDC__
/* You bet! */
#define __STDC__ 1
#endif

/* The IBM compiler uses the (non-standard) error code EACCESS instead of
   EACCES (note: one 'S'). Define EACCESS to be EACCES and use the standard
   name in the code. */
#ifndef EACCES
#define EACCES EACCESS
#endif

/* Handle some other name differences between the IBM and the Watcom
 * compiler.
 */
#ifdef __WATCOMC__
#define _setmode        setmode
#define _cwait          cwait
#endif

/* Some more WATCOM stuff: The watcom compiler defines va_list as an array,
 * not as a pointer, which will make the vasprintf code break without the
 * following define:
 */
#ifdef  __WATCOMC__
#define VA_LIST_IS_ARRAY
#endif

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#undef _ALL_SOURCE

/* Define to empty if the keyword does not work.  */
/* Const is working.  */
#undef const

/* Define to `int' if <sys/types.h> doesn't define.  */
/* OS/2 doesn't have gid_t.  It doesn't even really have group
   numbers, I think.  This will take more thought to get right, but
   let's get it running first.  */
#define gid_t int

/* Define if you support file names longer than 14 characters.  */
/* We support long file names, but not long corporate acronyms. */
#define HAVE_LONG_FILE_NAMES 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
/* If POSIX.1 requires this, why doesn't WNT have it?  */
/* Maybe POSIX only says that if it is present, it must behave a
   certain way, but that it can simply be not present too.  I
   dunno. */
/* Anyway, OS/2 ain't got it. */
#undef HAVE_SYS_WAIT_H

/* Define if utime(file, NULL) sets file's timestamp to the present.  */
/* Documentation says yup; haven't verified experimentally. */
#define HAVE_UTIME_NULL 1

/* Define if on MINIX.  */
/* Hah.  */
#undef _MINIX

/* Define to `int' if <sys/types.h> doesn't define.  */
#define mode_t int

/* Define to `int' if <sys/types.h> doesn't define.  */
#define pid_t int

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* This string doesn't appear anywhere in the system header files,
   so I assume it's irrelevant.  */
#undef _POSIX_1_SOURCE

/* Define if you need to in order for stat and other things to work.  */
/* Same as for _POSIX_1_SOURCE, above.  */
#undef _POSIX_SOURCE

/* Define as the return type of signal handlers (int or void).  */
/* IBMCPP manual indicates they are void.  */
#define RETSIGTYPE void

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* sys/types.h doesn't define it, but stdio.h does, which cvs.h
   #includes, so things should be okay.  */
/* #undef size_t */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly. */
/* sys/stat.h apparently doesn't even have them; setting this will let
   ../lib/system.h define them. */
#define STAT_MACROS_BROKEN 1
 
/* Define if you have the ANSI C header files.  */
/* We have at least a reasonable facsimile thereof. */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
/* We don't have <sys/time.h> at all.  Why isn't there a definition
   for HAVE_SYS_TIME_H anywhere in config.h.in?  */
#undef TIME_WITH_SYS_TIME

/* Define to `int' if <sys/types.h> doesn't define.  */
#define uid_t int

/* Define if you have MIT Kerberos version 4 available.  */
/* We don't. */
#undef HAVE_KERBEROS

/* Define if you want CVS to be able to be a remote repository client.  */
/* That's all we want.  */
#define CLIENT_SUPPORT

/* Define if you want CVS to be able to serve repositories to remote
   clients.  */
/* No server support yet.  Note that you don't have to define
   CLIENT_SUPPORT or SERVER_SUPPORT to enable the non-remote code;
   that's always there.  */
#undef SERVER_SUPPORT

/* Define if you have the connect function.  */
/* Not used?  */
/* It appears to be used in client.c now... don't know yet it OS/2 has it. */
#define HAVE_CONNECT

/* Define if you have the fchdir function.  */
#undef HAVE_FCHDIR

/* Define if you have the fchmod function.  */
#undef HAVE_FCHMOD

/* Define if you have the fsync function.  */
#undef HAVE_FSYNC

/* Define if you have the ftime function.  */
#define HAVE_FTIME 1

/* Define if you have the ftruncate function.  */
#undef HAVE_FTRUNCATE

/* Define if you have the getpagesize function.  */
#undef HAVE_GETPAGESIZE

/* Define if you have the krb_get_err_text function.  */
#undef HAVE_KRB_GET_ERR_TEXT

/* Define if you have the putenv function.  */
#define HAVE_PUTENV 1

/* Define if you have the sigaction function.  */
#undef HAVE_SIGACTION

/* Define if you have the sigblock function.  */
#undef HAVE_SIGBLOCK

/* Define if you have the sigprocmask function.  */
#undef HAVE_SIGPROCMASK

/* Define if you have the sigsetmask function.  */
#undef HAVE_SIGSETMASK

/* Define if you have the sigvec function.  */
#undef HAVE_SIGVEC

/* Define if you have the timezone function.  */
/* Hmm, I actually rather think it's an extern long
   variable; that message was mechanically generated
   by autoconf.  And I don't see any actual uses of
   this function in the code anyway, hmm.  */
#undef HAVE_TIMEZONE

/* Define if you have the tzset function.  */
#define HAVE_TZSET 1

/* Define if you have the vfork function.  */
#undef HAVE_VFORK

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the <direct.h> header file.  */
#define HAVE_DIRECT_H 1

/* Define if you have the <dirent.h> header file.  */
/* We have our own dirent.h and dirent.c. */
#ifdef __WATCOMC__
#undef HAVE_DIRENT_H
#else
#define HAVE_DIRENT_H 1
#endif

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <io.h> header file.  */
/* Low-level Unix I/O routines like open, creat, etc.  */
#define HAVE_IO_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <ndbm.h> header file.  */
#undef HAVE_NDBM_H

/* Define if you have the <ndir.h> header file.  */
#undef HAVE_NDIR_H

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/bsdtypes.h> header file.  */
#undef HAVE_SYS_BSDTYPES_H

/* Define if you have the <sys/dir.h> header file.  */
#undef HAVE_SYS_DIR_H

/* Define if you have the <sys/ndir.h> header file.  */
#undef HAVE_SYS_NDIR_H

/* Define if you have the <sys/param.h> header file.  */	
#undef HAVE_SYS_PARAM_H

/* Define if you have the <sys/select.h> header file.  */
#undef HAVE_SYS_SELECT_H

/* Define if you have the <sys/time.h> header file.  */
#undef HAVE_SYS_TIME_H

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the <utime.h> header file.  */
#undef HAVE_UTIME_H

/* Define if you have the <sys/utime.h> header file.  */
#define HAVE_SYS_UTIME_H 1

/* Define if you have the inet library (-linet).  */
#undef HAVE_LIBINET

/* Define if you have the nsl library (-lnsl).  */
/* This is not used anywhere in the source code.  */
#undef HAVE_LIBNSL

/* Define if you have the nsl_s library (-lnsl_s).  */
#undef HAVE_LIBNSL_S

/* Define if you have the socket library (-lsocket).  */
/* This isn't ever used either.  */
#undef HAVE_LIBSOCKET

/* Under OS/2, mkdir only takes one argument.  */
#define CVS_MKDIR os2_mkdir
extern int os2_mkdir (const char *PATH, int MODE);

/* OS/2 needs a special chdir functions that handles drives */
#define CVS_CHDIR os2_chdir
extern int os2_chdir (const char *Dir);

/* This function doesn't exist under OS/2; we provide a stub. */
extern int readlink (char *path, char *buf, int buf_size);

/* This is just a call to GetCurrentProcessID.  */
#ifndef __WATCOMC__
extern pid_t getpid (void);
#endif

/* We definitely have prototypes.  */
#define USE_PROTOTYPES 1

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

/* Make sure that we don't try to perform operations on RCS files on the
   local machine.  I think I neglected to apply some changes from
   MHI's port in that area of code, or found some issues I didn't want
   to deal with.  */
#define CLIENT_ONLY

/* We actually do have a transparent rsh, whew. */
#undef RSH_NOT_TRANSPARENT
/* But it won't be transparent unless we ask it nicely! */
#define RSH_NEEDS_BINARY_FLAG 1

/* OS/2 doesn't really have user/group permissions, at least not
   according to the C library manual pages.  So we'll make decoys.
   (This was partly introduced for an obsolete reason, now taken care
   of by CHMOD_BROKEN, but I haven't carefully looked at every case
   (in particular mode_to_string), so it might still be needed).
   We do not need that for the watcom compiler since watcom already
   all those permission bits defined. It would probably be better to
   include the necessary system header files in system.h, and then make
   each permission define only if it is not already defined.
*/
#ifndef __WATCOMC__
#define NEED_DECOY_PERMISSIONS 1     /* see system.h */
#endif



/* For the access() function, for which IBM OS/2 compiler has no pre-defined
   mnemonic masks. */
#ifndef __WATCOMC__
#define R_OK 04
#define W_OK 02
#define F_OK 00
#define X_OK R_OK  /* I think this is right for OS/2. */
#endif

/* For getpid() */
#include <process.h>

/* So "tcpip.h" gets included in lib/system.h: */
#define USE_OWN_TCPIP_H 1
/* The IBM TCP/IP library gets initialized in main(): */
#define SYSTEM_INITIALIZE(pargc,pargv) init_sockets()
extern void init_sockets();

/* Under OS/2, we have our own popen() and pclose()... */
#define USE_OWN_POPEN 1
/* ... and we use popenRW to start the rsh server. */
#define START_RSH_WITH_POPEN_RW 1

/*
 * This tells the client that it must use send()/recv() to talk to the
 * server if it is connected to the server via a socket.  Sigh.
 * Windows 95 and VMS cannot convert sockets to file descriptors either,
 * apparently.
 */
#define NO_SOCKET_TO_FD 1

/* chmod() doesn't seem to work -- IBM's own example program does not
 * behave as its documentation claims, in fact!  I suspect that
 * DosSetPathInfo is the way to go, but can't seem to make that work
 * either.  For now, we can deal with some cases by invoking the DOS
 * "attrib" command via system().  */
#define CHMOD_BROKEN 1

/* Rule Number 1 of OS/2 Programming: If the function you're looking
   for doesn't exist, try putting "Dos" in front of it.
   Do not forget to include the os2 header file if we use DosSleep. */
#ifndef sleep
#include "os2inc.h"
#define sleep(x) DosSleep(((long)(x))*1000L)
#endif /* sleep */

/* Set to 1 for some debugging messages. */
#if 0
#define KFF_DEBUG(call) printf("*** %s:%d: ", __FILE__, __LINE__); \
                        call; fflush(stdout);
#else
#define KFF_DEBUG(call)
#endif
