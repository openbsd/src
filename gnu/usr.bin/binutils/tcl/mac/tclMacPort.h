/*
 * tclMacPort.h --
 *
 *	This header file handles porting issues that occur because of
 *	differences between the Mac and Unix. It should be the only
 *	file that contains #ifdefs to handle different flavors of OS.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacPort.h 1.56 96/04/12 09:41:51
 */

#ifndef _MACPORT
#define _MACPORT

#ifndef _TCL
#include "tcl.h"
#endif

/* Includes */
#ifdef THINK_C
#   include <pascal.h>
#   include <posix.h>
#   include <string.h>
#   include <errno.h>
#   include <fcntl.h>
#   include <pwd.h>
#   include <sys/param.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <unistd.h>
#elif defined(__MWERKS__)
#   include <sys/types.h>

/*
 * We must explicitly include both errno.h and
 * sys/errno.h.  sys/errno.h is defined in CWGUSI
 */
#   include "errno.h"
#   include "sys/errno.h"

#   include <unistd.h>
#   include <sys/unistd.h>
#   include <sys/fcntl.h>
#   include <sys/stat.h>
#   define isatty(arg) 1

#endif

/*
 * waitpid doesn't work on a Mac - the following makes
 * Tcl compile without errors.  These would normally
 * be defined in sys/wait.h
 */
#ifdef NO_SYS_WAIT_H
#   define WNOHANG 1
#   define WIFSTOPPED(stat) (1)
#   define WIFSIGNALED(stat) (1)
#   define WIFEXITED(stat) (1)
#   define WIFSTOPSIG(stat) (1)
#   define WIFTERMSIG(stat) (1)
#   define WIFEXITSTATUS(stat) (1)
#   define WEXITSTATUS(stat) (1)
#   define WTERMSIG(status) (1)
#   define WSTOPSIG(status) (1)
#endif

/*
 * These defines are for functions that are now obsolete.  The only
 * Tcl code that still uses then is not called by the Mac.  The interfaces
 * should go away soon.
 */
#define TclOpenFile(fname, mode) ((Tcl_File)NULL)
#define TclCloseFile(file) (-1)
#define TclReadFile(file, shouldBlock, buf, toRead) (toRead)
#define TclWriteFile(file, shouldBlock, buf, toWrite) (-1)
#define TclSeekFile(file, offset, whence) (-1)

/*
 * The following macro defines the type of the mask arguments to
 * select:
 */

#ifndef NO_FD_SET
#   define SELECT_MASK fd_set
#else
#   ifndef _AIX
	typedef long fd_mask;
#   endif
#   if defined(_IBMR2)
#	define SELECT_MASK void
#   else
#	define SELECT_MASK int
#   endif
#endif

/*
 * Define "NBBY" (number of bits per byte) if it's not already defined.
 */

#ifndef NBBY
#   define NBBY 8
#endif

/*
 * The following macro defines the number of fd_masks in an fd_set:
 */

#if !defined(howmany)
#   define howmany(x, y) (((x)+((y)-1))/(y))
#endif
#ifdef NFDBITS
#   define MASK_SIZE howmany(FD_SETSIZE, NFDBITS)
#else
#   define MASK_SIZE howmany(OPEN_MAX, NBBY*sizeof(fd_mask))
#endif

/*
 * These functions always return dummy values on Mac.
 */
#define geteuid() 1
#define getpid() -1

/*
 * Macros to do string compares.  They pre-check the first character
 * before checking if the strings are equal.
 */

#define STREQU(str1, str2) \
        (((str1) [0] == (str2) [0]) && (strcmp (str1, str2) == 0))
#define STRNEQU(str1, str2, cnt) \
        (((str1) [0] == (str2) [0]) && (strncmp (str1, str2, cnt) == 0))

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define NO_SYS_ERRLIST

/*
 * Make sure that MAXPATHLEN is defined.
 */

#define WAIT_STATUS_TYPE pid_t

#ifndef MAXPATHLEN
#   ifdef PATH_MAX
#       define MAXPATHLEN PATH_MAX
#   else
#       define MAXPATHLEN 2048
#   endif
#endif

/*
 * The following functions are declared in tclInt.h but don't do anything
 * on Macintosh systems.
 */

#define TclSetSystemEnv(a,b)

/*
 * Many signals are not supported on the Mac and are thus not defined in
 * <signal.h>.  They are defined here so that Tcl will compile with less
 * modification.
  */

#ifndef SIGQUIT
#define SIGQUIT 300
#endif

#ifndef SIGPIPE
#define SIGPIPE 13
#endif

#ifndef SIGHUP
#define SIGHUP  100
#endif

/* A few error types are used by Tcl that don't make sense on a Mac */
/* They are defined here so that Tcl will compile with less modification */
#define ECHILD	10

extern char **environ;

/*
 * Prototypes needed for compatability
 */

EXTERN int 	TclMacCreateEnv _ANSI_ARGS_((void));
EXTERN FILE *	fdopen(int fd, const char *mode);
EXTERN int	fileno(FILE *stream);
EXTERN struct in_addr inet_addr(char *address);
EXTERN char *	inet_ntoa(struct in_addr inaddr);

#if (defined(THINK_C) || defined(__MWERKS__))
double		hypot(double x, double y);
#endif

/*
 * The following prototype and defines replace the Macintosh version
 * of the ANSI functions localtime and gmtime.  The various compilier
 * vendors to a simply horrible job of implementing these functions.
 */
EXTERN struct tm * TclMacSecondsToDate _ANSI_ARGS_((const time_t *time,
		    int useGMT));
#define localtime(time) TclMacSecondsToDate(time, 0)
#define gmtime(time) TclMacSecondsToDate(time, 1)

/*
 * The following prototypes and defines replace the Macintosh version
 * of the POSIX functions "stat" and "access".  The various compilier 
 * vendors don't implement this function well nor consistantly.
 */
EXTERN int TclMacStat _ANSI_ARGS_((char *path, struct stat *buf));
#define stat(path, bufPtr) TclMacStat(path, bufPtr)
#define lstat(path, bufPtr) TclMacStat(path, bufPtr)
EXTERN int TclMacAccess _ANSI_ARGS_((const char *filename, int mode));
#define access(path, mode) TclMacAccess(path, mode)
EXTERN FILE * TclMacFOpenHack _ANSI_ARGS_((const char *path,
	const char *mode));
#define fopen(path, mode) TclMacFOpenHack(path, mode)
#define readlink(fileName, buffer, size) -1

/*
 * Defines for Tcl internal commands that aren't really needed on
 * the Macintosh.  They all act as no-ops.
 */
#define TclCreateCommandChannel(out, in, err, num, pidPtr)	NULL
#define TclClosePipeFile(x)

/*
 * These definitions force putenv & company to use the version
 * supplied with Tcl.
 */
#ifndef putenv
#   define unsetenv	TclUnsetEnv
#   define putenv	Tcl_PutEnv
#   define setenv	TclSetEnv
void	TclSetEnv(CONST char *name, CONST char *value);
int	Tcl_PutEnv(CONST char *string);
void	TclUnsetEnv(CONST char *name);
#endif

/*
 * The default platform eol translation on Mac is TCL_TRANSLATE_CR:
 */

#define	TCL_PLATFORM_TRANSLATION	TCL_TRANSLATE_CR

#endif /* _MACPORT */
