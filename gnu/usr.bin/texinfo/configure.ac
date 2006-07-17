# Process this file with autoconf to produce a configure script.
# $Id: configure.ac,v 1.1.1.2 2006/07/17 16:03:10 espie Exp $
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
AC_PREREQ(2.59)# Minimum Autoconf version required.
#
AC_INIT([GNU Texinfo], [4.8])
AC_CONFIG_HEADERS(config.h:config.in)# Keep filename to 8.3 for MS-DOS.
AC_CONFIG_SRCDIR([makeinfo/makeinfo.c])
AM_INIT_AUTOMAKE([dist-bzip2])

# When the Texinfo source is imported into other repositories
# (NetBSD and TeX Live), timestamps are generally not preserved.  This
# causes lots of annoyance, so --enable-maintainer-mode.  Sorry.
AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AM_MISSING_PROG(HELP2MAN, help2man)

AC_ISC_POSIX
AC_MINIX

# Needed on sysV68 for sigblock, sigsetmask.  But check for it in libc first.
AC_CHECK_FUNC(sigblock, , AC_CHECK_LIB(bsd, sigblock))

# Checks for header files.
AC_HEADER_STAT
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h io.h limits.h pwd.h string.h strings.h \
  termcap.h termio.h termios.h unistd.h \
  sys/fcntl.h sys/file.h sys/stream.h sys/time.h sys/ttold.h sys/wait.h)

# sys/ptem.h requires sys/stream.h for mblk_t on Solaris.
AC_CHECK_HEADERS([sys/ptem.h],,,
[#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
])

AC_SYS_POSIX_TERMIOS
AC_HEADER_TIOCGWINSZ

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIGNAL
AC_C_CONST
AC_STRUCT_TM

# Checks for function declarations.
AC_CHECK_DECLS([memchr, strcoll, strerror])

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_STRCOLL
AC_FUNC_VPRINTF
# in theory only pre-sysvr3 systems needed this and it's not likely
# that anyone compiling new texinfo still has such a thing? we'll see.
# AC_FUNC_SETVBUF_REVERSED
AC_CHECK_FUNCS(bzero getcwd memset setvbuf sigaction sigprocmask \
               sigsetmask strchr)
AC_REPLACE_FUNCS(memcpy memmove strdup strerror)

# strcasecmp and strncasecmp, gnulib-style.
gl_STRCASE

# We want to recognize djgpp to avoid the useless warning about no
# term library.
AC_CANONICAL_BUILD

# We need to run some of our own binaries, most notably makedoc, but as
# long as we have this process, we also use our own makeinfo and
# install-info.
# 
# This means that if we are cross compiling, we have to configure the
# package twice: once with the native compiler (this is done in a
# subdirectory $native_tools), and once with the cross compiler.
# The former is invoked automatically here, with --host=$build.
# $native_tools is also added to SUBDIRS in the main Makefile.am,
# so that make compiles the native tools first.
#
if test "$cross_compiling" = no; then
  native_tools=
else
  native_tools=tools
  test -d "$native_tools" || mkdir "$native_tools"
  confdir=`(cd "$srcdir";pwd)`
  # Make sure the secondary configure won't fail with
  # "error: source directory already configured".
  rm -f config.status
  AC_MSG_NOTICE([[Doing configure of native tools (${build}).]])
  cd "$native_tools" || exit 1
  # Run secondary configure in alternate environment or
  # it gets the wrong CC etc.  env -i gives this build host configure
  # a clean environment.
  env -i CC="${BUILD_CC}" AR="${BUILD_AR}" RANLIB="${BUILD_RANLIB}" \
      PATH="${PATH}" \
      tools_only=1 \
    ${confdir}/configure --build=${build} --host=${build} \
                         --disable-rpath --disable-nls
  cd .. || exit 1
  AC_MSG_NOTICE([[Continuing with main configure (${host}).]])
fi
AC_SUBST(native_tools)
AM_CONDITIONAL(TOOLS_ONLY, [[test "x$tools_only" = x1]])

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

# Checks for variables.
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
[#ifdef HAVE_NCURSES_TERMCAP_H
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
   --Andreas Ley <andy@rz.uni-karlsruhe.de> 24 Aug 2000.  */
BC++;
UP++;
PC++;
return ospeed != 0;
], ac_cv_var_ospeed=$trylib; break)
done
LIBS=$oldLIBS
)
AC_MSG_RESULT($ac_cv_var_ospeed)
if test -n "$ac_cv_var_ospeed" \
   && test "x$termlib" != "x$ac_cv_var_ospeed"; then
  TERMLIBS="${TERMLIBS} -l${ac_cv_var_ospeed}"
fi
AC_SUBST(TERMLIBS)#

# Do not use <ncurses/termcap.h> unless we're linking with ncurses.
# Must come after the termlib tests.
if test "x$termlib" = xncurses; then
  # Use AC_CHECK_HEADERS so the HAVE_*_H symbol gets defined.
  AC_CHECK_HEADERS(ncurses/termcap.h)
fi

# gnulib.
gl_FUNC_MKSTEMP

# For teTeX and TeX Live.
AC_CANONICAL_HOST
AC_ARG_ENABLE(multiplatform,
  [  --enable-multiplatform      put executables in bin/PLATFORM])
# if enable_multiplatform is set in the environment, use that.
test "x$enable_multiplatform" = xyes \
&& test "x$bindir" = 'x${exec_prefix}/bin' \
&& bindir="$bindir/$host"

# Taken from the example in
# http://www.gnu.org/software/automake/manual/html_node/Conditionals.html.
AC_ARG_ENABLE(install-warnings,
  [  --disable-install-warnings  omit make install warnings about TeX files],
  [case "${enableval}" in
     yes) install_warnings=true ;;
      no) install_warnings=false ;;
       *) AC_MSG_ERROR(bad value ${enableval} for --enable-install-warnings) ;;
   esac],
   [install_warnings=true])
AM_CONDITIONAL(INSTALL_WARNINGS,
   test x"$install_warnings" = xtrue || test x"$install_warnings" = xyes)

# i18n support.  To update to a new version of gettext, run:
# gettextize -f -c --intl
AM_GNU_GETTEXT_VERSION(0.14.1)
AM_GNU_GETTEXT()

AC_CONFIG_FILES([
  Makefile
  doc/Makefile
  info/Makefile
  intl/Makefile
  lib/Makefile
  m4/Makefile
  makeinfo/Makefile
  makeinfo/tests/Makefile
  po/Makefile.in
  util/Makefile
])
AC_OUTPUT
