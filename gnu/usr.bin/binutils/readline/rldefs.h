/* rldefs.h -- an attempt to isolate some of the system-specific defines
   for readline.  This should be included after any files that define
   system-specific constants like _POSIX_VERSION or USG. */

/* Copyright (C) 1987,1989 Free Software Foundation, Inc.

   This file contains the Readline Library (the Library), a set of
   routines for providing Emacs style line input to programs that ask
   for it.

   The Library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   The Library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */

#if !defined (_RLDEFS_H)
#define _RLDEFS_H

#if defined (__GNUC__)
#  undef alloca
#  define alloca __builtin_alloca
#else
#  if defined (sparc) || defined (HAVE_ALLOCA_H)
#    include <alloca.h>
#  endif
#endif


#define NEW_TTY_DRIVER
#define HAVE_BSD_SIGNALS
/* #define USE_XON_XOFF */

/* Only do this for DOS, and WinGDB */
#if defined __MSDOS__ || (defined _WIN32 && !defined __CYGWIN32__)
#define NO_SYS_FILE
#undef NEW_TTY_DRIVER
#undef HAVE_BSD_SIGNALS
#define MINIMAL
#endif

/* Only do this for cygwin32 */
#if defined __CYGWIN32__
#define tgetent(ARG1, ARG2) -1
#define tgetnum(ARG1) ((int)abort())
#define tgetstr(ARG1, ARG2) ((char *)abort())
#define tgetflag(ARG1) ((int)abort())
#define tputs(ARG1, ARG2, ARG3) ((int)abort())
#define setpwent()
#else
extern char *tgetstr ();
#endif

#if defined (__linux__)
#  include <termios.h>
#  include <termcap.h>
#endif /* __linux__ */

/* Some USG machines have BSD signal handling (sigblock, sigsetmask, etc.) */
/* CYGNUS LOCAL accept __hpux as well as hpux for HP compiler in ANSI mode.  */
#if defined (USG) && !(defined (hpux) || defined (__hpux))
#  undef HAVE_BSD_SIGNALS
#endif

/* Only do this for WinGDB */
#if defined _WIN32 && !defined __CYGWIN32__
#define ScreenCols() 80
#define ScreenRows() 24
#define ScreenSetCursor() abort();
#define ScreenGetCursor() abort();
#endif

/* System V machines use termio. */
#if !defined (_POSIX_VERSION)
/* CYGNUS LOCAL accept __hpux as well as hpux for HP compiler in ANSI mode.
   Add __osf__ to list of machines to force use of termio.h */
#  if defined (USG) || defined (hpux) || defined (__hpux) || defined (Xenix) || defined (sgi) || defined (DGUX) || defined (__osf__)
#    undef NEW_TTY_DRIVER
#    define TERMIO_TTY_DRIVER
#    include <termio.h>
#    if !defined (TCOON)
#      define TCOON 1
#    endif
#  endif /* USG || hpux || Xenix || sgi || DUGX || __osf__ */
#endif /* !_POSIX_VERSION */

/* Posix systems use termios and the Posix signal functions. */
#if defined (_POSIX_VERSION)
#  if !defined (TERMIOS_MISSING)
#    undef NEW_TTY_DRIVER
#    define TERMIOS_TTY_DRIVER
#    include <termios.h>
#  endif /* !TERMIOS_MISSING */
#  define HAVE_POSIX_SIGNALS
#  if !defined (O_NDELAY)
#    define O_NDELAY O_NONBLOCK	/* Posix-style non-blocking i/o */
#  endif /* O_NDELAY */
#endif /* _POSIX_VERSION */

/* System V.3 machines have the old 4.1 BSD `reliable' signal interface. */
#if !defined (HAVE_BSD_SIGNALS) && !defined (HAVE_POSIX_SIGNALS)
#  if defined (USGr3)
#    if !defined (HAVE_USG_SIGHOLD)
#      define HAVE_USG_SIGHOLD
#    endif /* !HAVE_USG_SIGHOLD */
#  endif /* USGr3 */
#endif /* !HAVE_BSD_SIGNALS && !HAVE_POSIX_SIGNALS */

/* Other (BSD) machines use sgtty. */
#if defined (NEW_TTY_DRIVER)
#  include <sgtty.h>
#endif

/* Define _POSIX_VDISABLE if we are not using the `new' tty driver and
   it is not already defined.  It is used both to determine if a
   special character is disabled and to disable certain special
   characters.  Posix systems should set to 0, USG systems to -1. */
#if !defined (NEW_TTY_DRIVER) && !defined (_POSIX_VDISABLE)
#  if defined (_POSIX_VERSION)
#    define _POSIX_VDISABLE 0
#  else /* !_POSIX_VERSION */
#    define _POSIX_VDISABLE -1
#  endif /* !_POSIX_VERSION */
#endif /* !NEW_TTY_DRIVER && !_POSIX_VDISABLE */

#if 1
#  define D_NAMLEN(d) strlen ((d)->d_name)
#else /* !1 */

#if !defined (SHELL) && (defined (_POSIX_VERSION) || defined (USGr3))
#  if !defined (HAVE_DIRENT_H)
#    define HAVE_DIRENT_H
#  endif /* !HAVE_DIRENT_H */
#endif /* !SHELL && (_POSIX_VERSION || USGr3) */

#if defined (HAVE_DIRENT_H)
#  include <dirent.h>
#  if !defined (direct)
#    define direct dirent
#  endif /* !direct */
#  define D_NAMLEN(d) strlen ((d)->d_name)
#else /* !HAVE_DIRENT_H */
#  define D_NAMLEN(d) ((d)->d_namlen)
#  if defined (USG)
#    if defined (Xenix)
#      include <sys/ndir.h>
#    else /* !Xenix (but USG...) */
#      include "ndir.h"
#    endif /* !Xenix */
#  else /* !USG */
#    include <sys/dir.h>
#  endif /* !USG */
#endif /* !HAVE_DIRENT_H */
#endif /* !1 */

#if defined (USG) && defined (TIOCGWINSZ) && !defined (Linux)
#  if defined (_AIX)
	/* AIX 4.x seems to reference struct uio within a prototype
	   in stream.h, but doesn't cause the uio include file to
	   be included.  */
#    include <sys/uio.h>
#  endif
#  include <sys/stream.h>
#  if defined (HAVE_SYS_PTEM_H)
#    include <sys/ptem.h>
#  endif /* HAVE_SYS_PTEM_H */
#  if defined (HAVE_SYS_PTE_H)
#    include <sys/pte.h>
#  endif /* HAVE_SYS_PTE_H */
#endif /* USG && TIOCGWINSZ && !Linux */

/* Posix macro to check file in statbuf for directory-ness.
   This requires that <sys/stat.h> be included before this test. */
#if defined (S_IFDIR) && !defined (S_ISDIR)
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#endif
/* Posix macro to check file in statbuf for file-ness.
   This requires that <sys/stat.h> be included before this test. */
#if defined (S_IFREG) && !defined (S_ISREG)
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#endif

#if !defined (strchr) && !defined (__STDC__)
extern char *strchr (), *strrchr ();
#endif /* !strchr && !__STDC__ */

#if defined (HAVE_VARARGS_H)
#  include <varargs.h>
#endif /* HAVE_VARARGS_H */

/* This definition is needed by readline.c, rltty.c, and signals.c. */
/* If on, then readline handles signals in a way that doesn't screw. */
#define HANDLE_SIGNALS

#if defined __MSDOS__ || (defined _WIN32 && !defined __CYGWIN32__)
#undef HANDLE_SIGNALS
#endif


#if !defined (emacs_mode)
#  define no_mode -1
#  define vi_mode 0
#  define emacs_mode 1
#endif

/* Define some macros for dealing with assorted signalling disciplines.

   These macros provide a way to use signal blocking and disabling
   without smothering your code in a pile of #ifdef's.

   SIGNALS_UNBLOCK;			Stop blocking all signals.

   {
     SIGNALS_DECLARE_SAVED (name);	Declare a variable to save the 
					signal blocking state.
	...
     SIGNALS_BLOCK (SIGSTOP, name);	Block a signal, and save the previous
					state for restoration later.
	...
     SIGNALS_RESTORE (name);		Restore previous signals.
   }

*/

#ifdef HAVE_POSIX_SIGNALS
							/* POSIX signals */

#define	SIGNALS_UNBLOCK \
      do { sigset_t set;	\
	sigemptyset (&set);	\
	sigprocmask (SIG_SETMASK, &set, (sigset_t *)NULL);	\
      } while (0)

#define	SIGNALS_DECLARE_SAVED(name)	sigset_t name

#define	SIGNALS_BLOCK(SIG, saved)	\
	do { sigset_t set;		\
	  sigemptyset (&set);		\
	  sigaddset (&set, SIG);	\
	  sigprocmask (SIG_BLOCK, &set, &saved);	\
	} while (0)

#define	SIGNALS_RESTORE(saved)		\
  sigprocmask (SIG_SETMASK, &saved, (sigset_t *)NULL)


#else	/* HAVE_POSIX_SIGNALS */
#ifdef HAVE_BSD_SIGNALS
							/* BSD signals */

#define	SIGNALS_UNBLOCK			sigsetmask (0)
#define	SIGNALS_DECLARE_SAVED(name)	int name
#define	SIGNALS_BLOCK(SIG, saved)	saved = sigblock (sigmask (SIG))
#define	SIGNALS_RESTORE(saved)		sigsetmask (saved)


#else  /* HAVE_BSD_SIGNALS */
							/* None of the Above */

#define	SIGNALS_UNBLOCK			/* nothing */
#define	SIGNALS_DECLARE_SAVED(name)	/* nothing */
#define	SIGNALS_BLOCK(SIG, saved)	/* nothing */
#define	SIGNALS_RESTORE(saved)		/* nothing */


#endif /* HAVE_BSD_SIGNALS */
#endif /* HAVE_POSIX_SIGNALS */

#if !defined (strchr)
extern char *strchr ();
#endif
#if !defined (strrchr)
extern char *strrchr ();
#endif
#ifdef __STDC__
#include <stddef.h>
extern size_t strlen (const char *s);
#endif  /* __STDC__ */

/*  End of signal handling definitions.  */
#endif /* !_RLDEFS_H */
