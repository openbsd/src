dnl Process this file with autoconf to produce a configure script.
dnl $Id: configure.ac,v 1.1.1.1 2002/06/10 13:20:14 espie Exp $
dnl
AC_PREREQ(2.52)dnl Minimum Autoconf version required.
dnl
AC_INIT([GNU Texinfo], [4.2])
AC_CONFIG_SRCDIR([makeinfo/makeinfo.c])
AM_CONFIG_HEADER(config.h:config.in)dnl Keep filename to 8.3 for MS-DOS.
AM_INIT_AUTOMAKE
dnl
AM_CONDITIONAL(TEXINFO_MAINT, test -n "$TEXINFO_MAINT")

dnl Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_PROG_INSTALL
AC_PROG_RANLIB

AC_ISC_POSIX
AC_MINIX

# Needed on sysV68 for sigblock, sigsetmask.  But check for it in libc first.
AC_CHECK_FUNC(sigblock, , AC_CHECK_LIB(bsd, sigblock))

dnl Checks for header files.
dnl Do not use <ncurses/termcap.h> unless we're linking with ncurses.
if test "x$termlib" = xncurses; then
  dnl Use AC_CHECK_HEADERS so the HAVE_*_H symbol gets defined.
  AC_CHECK_HEADERS(ncurses/termcap.h)
fi
AC_HEADER_STAT
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h io.h limits.h pwd.h string.h strings.h \
  termcap.h termio.h termios.h unistd.h \
  sys/fcntl.h sys/file.h sys/ptem.h sys/time.h sys/ttold.h sys/wait.h)
AC_SYS_POSIX_TERMIOS
AC_HEADER_TIOCGWINSZ

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIGNAL
AC_C_CONST
AC_STRUCT_TM

dnl Checks for function declarations.
txi_CHECK_DECLS

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_STRCOLL
AC_FUNC_VPRINTF
dnl in theory only pre-sysvr3 systems needed this and it's not likely
dnl that anyone compiling new texinfo still has such a thing? we'll see.
dnl AC_FUNC_SETVBUF_REVERSED
AC_CHECK_FUNCS(setvbuf getcwd memset bzero strchr sigprocmask sigsetmask)
dnl strerror, xmalloc, xrealloc, probably others should be added.
AC_REPLACE_FUNCS(memcpy memmove strdup strcasecmp strerror strncasecmp)

dnl We want to recognize djgpp to avoid the useless warning about no
dnl term library.
AC_CANONICAL_BUILD

# Some GNU/Linux systems (e.g., SuSE 4.3, 1996) don't have curses, but
# rather ncurses.  So we check for it.
TERMLIBS=
# Check for termlib before termcap because Solaris termcap needs libucb.
TERMLIB_VARIANTS="ncurses curses termlib termcap terminfo"
for termlib in ${TERMLIB_VARIANTS}; do
  AC_CHECK_LIB(${termlib}, tgetent,
     [TERMLIBS="${TERMLIBS} -l${termlib}"; break])
done
# don't bother warning on djgpp, it doesn't have a term library, it
# ports each termcap-needing program separately according to its needs.
if test -z "$TERMLIBS" && echo "$build" | grep -v djgpp >/dev/null; then
  AC_MSG_WARN([probably need a terminal library, one of: ${TERMLIB_VARIANTS}])
fi

dnl Checks for variables.
# HP-UX 9 (at least) needs -lncurses which defines termcap variables PC etc.
AC_MSG_CHECKING(for library with termcap variables)
AC_CACHE_VAL(ac_cv_var_ospeed,
oldLIBS=$LIBS 
for trylib in $termlib ${TERMLIB_VARIANTS}; do
   if test "x$trylib" != "x$termlib"; then
     LIBS="$oldLIBS -l$termlib -l$trylib"
   else
     LIBS="$oldLIBS -l$termlib"
   fi
   AC_TRY_LINK(, 
#ifdef HAVE_NCURSES_TERMCAP_H
#include <ncurses/termcap.h>
#else
#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#else
#undef PC
char *BC;
char **UP;
char PC;      
short ospeed; 
#endif
#endif
/* Make sure all variables actually exist.  AIX 4.3 has ospeed but no BC.
   --Andreas Ley <andy@rz.uni-karlsruhe.de>, 24 Aug 2000.  */
BC++;
UP++
PC++;
return ospeed != 0;
, ac_cv_var_ospeed=$trylib; break)
done
LIBS=$oldLIBS
)
AC_MSG_RESULT($ac_cv_var_ospeed)
if test -n "$ac_cv_var_ospeed" \
   && test "x$termlib" != "x$ac_cv_var_ospeed"; then
  TERMLIBS="${TERMLIBS} -l${ac_cv_var_ospeed}"
fi
AC_SUBST(TERMLIBS)dnl

dnl i18n support.
AM_GNU_GETTEXT()

AC_CONFIG_FILES([Makefile          \
           doc/Makefile            \
           info/Makefile           \
           intl/Makefile	   \
           lib/Makefile            \
           m4/Makefile		   \
           makeinfo/Makefile       \
           makeinfo/tests/Makefile \
           po/Makefile.in	   \
           util/Makefile           \
           ])
AC_OUTPUT
