/*	$OpenBSD: config.h,v 1.2 1996/08/25 11:56:34 downsj Exp $	*/

/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/*
 * This file, acconfig.h, which is a part of pdksh (the public domain ksh),
 * is placed in the public domain.  It comes with no licence, warranty
 * or guarantee of any kind (i.e., at your own risk).
 */

#ifndef CONFIG_H
#define CONFIG_H


/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if the closedir function returns void instead of int.  */
/* #undef CLOSEDIR_VOID */

/* Define to empty if the keyword does not work.  */
/* #undef const */


/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if you have a working `mmap' system call.  */
#define HAVE_MMAP 1

/* Define if your struct stat has st_rdev.  */
#define HAVE_ST_RDEV 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have <unistd.h>.  */
#define HAVE_UNISTD_H 1

/* Define if on MINIX.  */
/* #undef _MINIX */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if `sys_siglist' is declared by <signal.h>.  */
#define SYS_SIGLIST_DECLARED 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define if the closedir function returns void instead of int.  */
/* #undef VOID_CLOSEDIR */

/* Define if your kernal doesn't handle scripts starting with #! */
/* #undef SHARPBANG */

/* Define if dup2() preserves the close-on-exec flag (ultrix does this) */
/* #undef DUP2_BROKEN */

/* Define as the return value of signal handlers (0 or ).  */
#define RETSIGVAL 

/* Define if you have posix signal routines (sigaction(), et. al.) */
#define POSIX_SIGNALS 1

/* Define if you have BSD4.2 signal routines (sigsetmask(), et. al.) */
/* #undef BSD42_SIGNALS */

/* Define if you have BSD4.1 signal routines (sigset(), et. al.) */
/* #undef BSD41_SIGNALS */

/* Define if you have v7 signal routines (signal(), signal reset on delivery) */
/* #undef V7_SIGNALS */

/* Define to use the fake posix signal routines (sigact.[ch]) */
/* #undef USE_FAKE_SIGACT */

/* Define if signals don't interrupt read() */
/* #undef SIGNALS_DONT_INTERRUPT */

/* Define if you have bsd versions of the setpgrp() and getpgrp() routines */
/* #undef BSD_PGRP */

/* Define if you have POSIX versions of the setpgid() and getpgrp() routines */
#define POSIX_PGRP 1

/* Define if you have sysV versions of the setpgrp() and getpgrp() routines */
/* #undef SYSV_PGRP */

/* Define if you don't have setpgrp(), setpgid() or getpgrp() routines */
/* #undef NO_PGRP */

/* Define to char if your compiler doesn't like the void keyword */
/* #undef void */

/* Define to nothing if compiler doesn't like the volatile keyword */
/* #undef volatile */

/* Define if C compiler groks function prototypes */
#define HAVE_PROTOTYPES 1

/* Define if C compiler groks __attribute__((...)) (const, noreturn, format) */
#define HAVE_GCC_FUNC_ATTR 1

/* Define to 32-bit signed integer type if <sys/types.h> doesn't define */
/* #undef clock_t */

/* Define to the type of struct rlimit fields if the rlim_t type is missing */
/* #undef rlim_t */

/* Define if time() is declared in <time.h> */
#define TIME_DECLARED 1

/* Define to `unsigned' if <signal.h> doesn't define */
/* #undef sigset_t */

/* Define if sys_errlist[] and sys_nerr are in the C library */
#define HAVE_SYS_ERRLIST 1

/* Define if sys_errlist[] and sys_nerr are defined in <errno.h> */
#define SYS_ERRLIST_DECLARED 1

/* Define if sys_siglist[] is in the C library */
#define HAVE_SYS_SIGLIST 1

/* Define if you have a sane <termios.h> header file */
#define HAVE_TERMIOS_H 1

/* Define if you have a memset() function in your C library */
#define HAVE_MEMSET 1

/* Define if you have a memmove() function in your C library */
#define HAVE_MEMMOVE 1

/* Define if you have a bcopy() function in your C library */
/* #undef HAVE_BCOPY */

/* Define if you have a lstat() function in your C library */
#define HAVE_LSTAT 1

/* Define if you have a sane <termio.h> header file */
/* #undef HAVE_TERMIO_H */

/* Define if you don't have times() or if it always returns 0 */
/* #undef TIMES_BROKEN */

/* Define if opendir() will open non-directory files */
/* #undef OPENDIR_DOES_NONDIR */

/* Define if the pgrp of setpgrp() can't be the pid of a zombie process */
/* #undef NEED_PGRP_SYNC */

/* Define if you arg running SCO unix */
/* #undef OS_SCO */

/* Define if you arg running ISC unix */
/* #undef OS_ISC */

/* Define if you arg running OS2 with the EMX library */
/* #undef OS2 */

/* Define if you have a POSIX.1 compatiable <sys/wait.h> */
#define POSIX_SYS_WAIT 1

/* Define if your OS maps references to /dev/fd/n to file descriptor n */
#define HAVE_DEV_FD 1

/* Default PATH (see comments in configure.in for more details) */
#define DEFAULT_PATH "/bin:/usr/bin:/usr/ucb"

/* Include ksh features? (see comments in configure.in for more details) */
/* #define KSH 1 */

/* Include emacs editing? (see comments in configure.in for more details) */
#define EMACS 1

/* Include vi editing? (see comments in configure.in for more details) */
#define VI 1

/* Include job control? (see comments in configure.in for more details) */
#define JOBS 1

/* Include brace-expansion? (see comments in configure.in for more details) */
#define BRACE_EXPAND 1

/* Include any history? (see comments in configure.in for more details) */
#define HISTORY 1

/* Include complex history? (see comments in configure.in for more details) */
/* #undef COMPLEX_HISTORY */

/* Strict POSIX behaviour? (see comments in configure.in for more details) */
/* #undef POSIXLY_CORRECT */

/* Specify default $ENV? (see comments in configure.in for more details) */
/* #undef DEFAULT_ENV */

/* Include shl(1) support? (see comments in configure.in for more details) */
/* #undef SWTCH */

/* Include game-of-life? (see comments in configure.in for more details) */
/* #undef SILLY */

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/* The number of bytes in a long.  */
#if defined(alpha)
#define SIZEOF_LONG 8
#else
#define SIZEOF_LONG 4
#endif

/* Define if you have the _setjmp function.  */
/* #undef HAVE__SETJMP */

/* Define if you have the confstr function.  */
#define HAVE_CONFSTR 1

/* Define if you have the flock function.  */
#define HAVE_FLOCK 1

/* Define if you have the getgroups function.  */
/* #undef HAVE_GETGROUPS */

/* Define if you have the getrusage function.  */
/* #undef HAVE_GETRUSAGE */

/* Define if you have the getwd function.  */
#define HAVE_GETWD 1

/* Define if you have the killpg function.  */
#define HAVE_KILLPG 1

/* Define if you have the nice function.  */
#define HAVE_NICE 1

/* Define if you have the setrlimit function.  */
#define HAVE_SETRLIMIT 1

/* Define if you have the sigsetjmp function.  */
#define HAVE_SIGSETJMP 1

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the sysconf function.  */
#define HAVE_SYSCONF 1

/* Define if you have the tcsetpgrp function.  */
#define HAVE_TCSETPGRP 1

/* Define if you have the ulimit function.  */
/* #undef HAVE_ULIMIT */

/* Define if you have the wait3 function.  */
#define HAVE_WAIT3 1

/* Define if you have the waitpid function.  */
#define HAVE_WAITPID 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <paths.h> header file.  */
#define HAVE_PATHS_H 1

/* Define if you have the <stddef.h> header file.  */
#define HAVE_STDDEF_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/wait.h> header file.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have the <ulimit.h> header file.  */
/* #undef HAVE_ULIMIT_H */

/* Define if you have the <values.h> header file.  */
/* #undef HAVE_VALUES_H */

/* Need to use a separate file to keep the configure script from commenting
 * out the undefs....
 */
#include "conf-end.h"

#endif /* CONFIG_H */
