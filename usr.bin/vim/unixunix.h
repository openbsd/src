/*	$OpenBSD: unixunix.h,v 1.1.1.1 1996/09/07 21:40:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * unixunix.h -- include files that are only used in unix.c
 */

/*
 * Stuff for signals
 */
#ifdef HAVE_SIGSET
# define signal sigset
#endif

   /* sun's sys/ioctl.h redefines symbols from termio world */
#if defined(HAVE_SYS_IOCTL_H) && !defined(sun)
# include <sys/ioctl.h>
#endif

#ifndef USE_SYSTEM		/* use fork/exec to start the shell */

# if defined(HAVE_SYS_WAIT_H) || defined(HAVE_UNION_WAIT)
#  include <sys/wait.h>
# endif

#if defined(HAVE_SYS_SELECT_H) && \
		(!defined(HAVE_SYS_TIME_H) || defined(SYS_SELECT_WITH_SYS_TIME))
#  include <sys/select.h>
# endif

# ifndef WEXITSTATUS
#  ifdef HAVE_UNION_WAIT
#   define WEXITSTATUS(stat_val) ((stat_val).w_T.w_Retcode)
#  else
#   define WEXITSTATUS(stat_val) (((stat_val) >> 8) & 0377)
#  endif
# endif

# ifndef WIFEXITED
#  ifdef HAVE_UNION_WAIT
#   define WIFEXITED(stat_val) ((stat_val).w_T.w_Termsig == 0)
#  else
#   define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#  endif
# endif

#endif /* !USE_SYSTEM */

#ifdef HAVE_STROPTS_H
# include <stropts.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifndef HAVE_SELECT
# ifdef HAVE_SYS_POLL_H
#  include <sys/poll.h>
# else
#  include <poll.h>
# endif
#endif

#ifdef HAVE_SYS_STREAM_H
# include <sys/stream.h>
#endif

#ifdef HAVE_SYS_PTEM_H
# include <sys/ptem.h>
# ifndef _IO_PTEM_H			/* For UnixWare that should check for _IO_PT_PTEM_H */
#  define _IO_PTEM_H
# endif
#endif

#ifdef HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif

#ifdef HAVE_SYS_SYSTEMINFO_H
/*
 * foolish Sinix <sys/systeminfo.h> uses SYS_NMLN but doesn't include
 * limits.h>, where it is defined. Perhaps other systems have the same
 * problem? Include it here. -- Slootman
 */
# if defined(HAVE_LIMITS_H) && !defined(_LIMITS_H)
#  include <limits.h>           /* for SYS_NMLN (Sinix 5.41 / Unix SysV.4) */
# endif
# include <sys/systeminfo.h>	/* for sysinfo */
#endif

/*
 * We use termios.h if both termios.h and termio.h are available.
 * Termios is supposed to be a superset of termio.h.  Don't include them both,
 * it may give problems on some systems (e.g. hpux).
 * I don't understand why we don't want termios.h for apollo.
 */
#if defined(HAVE_TERMIOS_H) && !defined(apollo)
#  include <termios.h>
#else
# ifdef HAVE_TERMIO_H
#  include <termio.h>
# else
#  ifdef HAVE_SGTTY_H
#   include <sgtty.h>
#  endif
# endif
#endif
