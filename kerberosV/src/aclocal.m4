# ./aclocal.m4 generated automatically by aclocal 1.4b

# Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000
# Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

dnl $KTH: acinclude.m4,v 1.15 1998/05/23 14:54:53 joda Exp $
dnl
dnl Only put things that for some reason can't live in the `cf'
dnl directory in this file.
dnl

dnl $KTH: misc.m4,v 1.1 1997/12/14 15:59:04 joda Exp $
dnl
define(upcase,`echo $1 | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ`)dnl

dnl $KTH: misc.m4,v 1.2 2000/07/19 15:04:00 joda Exp $
dnl
AC_DEFUN([upcase],[`echo $1 | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ`])dnl
AC_DEFUN([rk_CONFIG_HEADER],[AH_TOP([#ifndef RCSID
#define RCSID(msg) \
static /**/const char *const rcsid[] = { (const char *)rcsid, "\100(#)" msg }
#endif

#undef BINDIR 
#undef LIBDIR
#undef LIBEXECDIR
#undef SBINDIR

#undef HAVE_INT8_T
#undef HAVE_INT16_T
#undef HAVE_INT32_T
#undef HAVE_INT64_T
#undef HAVE_U_INT8_T
#undef HAVE_U_INT16_T
#undef HAVE_U_INT32_T
#undef HAVE_U_INT64_T

/* Maximum values on all known systems */
#define MaxHostNameLen (64+4)
#define MaxPathLen (1024+4)

])])
# Like AC_CONFIG_HEADER, but automatically create stamp file.

# serial 3

# When config.status generates a header, we must update the stamp-h file.
# This file resides in the same directory as the config header
# that is generated.  We must strip everything past the first ":",
# and everything past the last "/".

AC_PREREQ([2.12])

AC_DEFUN([AM_CONFIG_HEADER],
[AC_CONFIG_HEADER([$1])
  AC_OUTPUT_COMMANDS(
   ifelse(patsubst([$1], [[^ ]], []),
	  [],
	  [test -z "$CONFIG_HEADERS" || echo timestamp >dnl
	   patsubst([$1], [^\([^:]*/\)?.*], [\1])stamp-h]),
  [am_indx=1
  for am_file in $1; do
    case " $CONFIG_HEADERS " in
    *" $am_file "*)
      echo timestamp > `echo $am_file | sed 's%:.*%%;s%[^/]*$%%'`stamp-h$am_indx
      ;;
    esac
    am_indx=\`expr \$am_indx + 1\`
  done])
])

# Do all the work for Automake.  This macro actually does too much --
# some checks are only needed if your package does certain things.
# But this isn't really a big deal.

# serial 3

AC_PREREQ([2.13])

# AC_PROVIDE_IFELSE(MACRO-NAME, IF-PROVIDED, IF-NOT-PROVIDED)
# -----------------------------------------------------------
# If MACRO-NAME is provided do IF-PROVIDED, else IF-NOT-PROVIDED.
# The purpose of this macro is to provide the user with a means to
# check macros which are provided without letting her know how the
# information is coded.
# If this macro is not defined by Autoconf, define it here.
ifdef([AC_PROVIDE_IFELSE],
      [],
      [define([AC_PROVIDE_IFELSE],
              [ifdef([AC_PROVIDE_$1],
                     [$2], [$3])])])


# AM_INIT_AUTOMAKE(PACKAGE,VERSION, [NO-DEFINE])
# ----------------------------------------------
AC_DEFUN([AM_INIT_AUTOMAKE],
[dnl We require 2.13 because we rely on SHELL being computed by configure.
AC_REQUIRE([AC_PROG_INSTALL])dnl
# test to see if srcdir already configured
if test "`CDPATH=:; cd $srcdir && pwd`" != "`pwd`" &&
   test -f $srcdir/config.status; then
  AC_MSG_ERROR([source directory already configured; run "make distclean" there first])
fi

# Define the identity of the package.
PACKAGE=$1
AC_SUBST(PACKAGE)dnl
VERSION=$2
AC_SUBST(VERSION)dnl
ifelse([$3],,
[AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package])])

# Some tools Automake needs.
AC_REQUIRE([AM_SANITY_CHECK])dnl
AC_REQUIRE([AC_ARG_PROGRAM])dnl
AM_MISSING_PROG(ACLOCAL, aclocal)
AM_MISSING_PROG(AUTOCONF, autoconf)
AM_MISSING_PROG(AUTOMAKE, automake)
AM_MISSING_PROG(AUTOHEADER, autoheader)
AM_MISSING_PROG(MAKEINFO, makeinfo)
AM_MISSING_PROG(AMTAR, tar)
AM_MISSING_INSTALL_SH
# We need awk for the "check" target.  The system "awk" is bad on
# some platforms.
AC_REQUIRE([AC_PROG_AWK])dnl
AC_REQUIRE([AC_PROG_MAKE_SET])dnl
AC_REQUIRE([AM_DEP_TRACK])dnl
AC_REQUIRE([AM_SET_DEPDIR])dnl
AC_PROVIDE_IFELSE([AC_PROG_CC],
                  [AM_DEPENDENCIES(CC)],
                  [define([AC_PROG_CC],
                          defn([AC_PROG_CC])[AM_DEPENDENCIES(CC)])])dnl
AC_PROVIDE_IFELSE([AC_PROG_CXX],
                  [AM_DEPENDENCIES(CXX)],
                  [define([AC_PROG_CXX],
                          defn([AC_PROG_CXX])[AM_DEPENDENCIES(CXX)])])dnl
])

#
# Check to make sure that the build environment is sane.
#

AC_DEFUN([AM_SANITY_CHECK],
[AC_MSG_CHECKING([whether build environment is sane])
# Just in case
sleep 1
echo timestamp > conftestfile
# Do `set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   set X `ls -Lt $srcdir/configure conftestfile 2> /dev/null`
   if test "[$]*" = "X"; then
      # -L didn't work.
      set X `ls -t $srcdir/configure conftestfile`
   fi
   if test "[$]*" != "X $srcdir/configure conftestfile" \
      && test "[$]*" != "X conftestfile $srcdir/configure"; then

      # If neither matched, then we have a broken ls.  This can happen
      # if, for instance, CONFIG_SHELL is bash and it inherits a
      # broken ls alias from the environment.  This has actually
      # happened.  Such a system could not be considered "sane".
      AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken
alias in your environment])
   fi

   test "[$]2" = conftestfile
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
rm -f conftest*
AC_MSG_RESULT(yes)])

# AM_MISSING_PROG(NAME, PROGRAM)
AC_DEFUN([AM_MISSING_PROG], [
AC_REQUIRE([AM_MISSING_HAS_RUN])
$1=${$1-"${am_missing_run}$2"}
AC_SUBST($1)])

# Like AM_MISSING_PROG, but only looks for install-sh.
# AM_MISSING_INSTALL_SH()
AC_DEFUN([AM_MISSING_INSTALL_SH], [
AC_REQUIRE([AM_MISSING_HAS_RUN])
if test -z "$install_sh"; then
   install_sh="$ac_aux_dir/install-sh"
   test -f "$install_sh" || install_sh="$ac_aux_dir/install.sh"
   test -f "$install_sh" || install_sh="${am_missing_run}${ac_auxdir}/install-sh"
   dnl FIXME: an evil hack: we remove the SHELL invocation from
   dnl install_sh because automake adds it back in.  Sigh.
   install_sh="`echo $install_sh | sed -e 's/\${SHELL}//'`"
fi
AC_SUBST(install_sh)])

# AM_MISSING_HAS_RUN.
# Define MISSING if not defined so far and test if it supports --run.
# If it does, set am_missing_run to use it, otherwise, to nothing.
AC_DEFUN([AM_MISSING_HAS_RUN], [
test x"${MISSING+set}" = xset || \
  MISSING="\${SHELL} `CDPATH=:; cd $ac_aux_dir && pwd`/missing"
# Use eval to expand $SHELL
if eval "$MISSING --run :"; then
  am_missing_run="$MISSING --run "
else
  am_missing_run=
  am_backtick='`'
  AC_MSG_WARN([${am_backtick}missing' script is too old or missing])
fi
])

# See how the compiler implements dependency checking.
# Usage:
# AM_DEPENDENCIES(NAME)
# NAME is "CC", "CXX" or "OBJC".

# We try a few techniques and use that to set a single cache variable.

AC_DEFUN([AM_DEPENDENCIES],[
AC_REQUIRE([AM_SET_DEPDIR])
AC_REQUIRE([AM_OUTPUT_DEPENDENCY_COMMANDS])
ifelse([$1],CC,[
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AC_PROG_CPP])
depcc="$CC"
depcpp="$CPP"],[$1],CXX,[
AC_REQUIRE([AC_PROG_CXX])
AC_REQUIRE([AC_PROG_CXXCPP])
depcc="$CXX"
depcpp="$CXXCPP"],[$1],OBJC,[
am_cv_OBJC_dependencies_compiler_type=gcc],[
AC_REQUIRE([AC_PROG_][$1])
depcc="$[$1]"
depcpp=""])
AC_MSG_CHECKING([dependency style of $depcc])
AC_CACHE_VAL(am_cv_[$1]_dependencies_compiler_type,[
if test -z "$AMDEP"; then
  echo '#include "conftest.h"' > conftest.c
  echo 'int i;' > conftest.h

  am_cv_[$1]_dependencies_compiler_type=none
  for depmode in `sed -n 's/^#*\([a-zA-Z0-9]*\))$/\1/p' < "$am_depcomp"`; do
    case "$depmode" in
    nosideeffect)
      # after this tag, mechanisms are not by side-effect, so they'll
      # only be used when explicitly requested
      if test "x$enable_dependency_tracking" = xyes; then
	continue
      else
	break
      fi
      ;;
    none) break ;;
    esac
    # We check with `-c' and `-o' for the sake of the "dashmstdout"
    # mode.  It turns out that the SunPro C++ compiler does not properly
    # handle `-M -o', and we need to detect this.
    if depmode="$depmode" \
       source=conftest.c object=conftest.o \
       depfile=conftest.Po tmpdepfile=conftest.TPo \
       $SHELL $am_depcomp $depcc -c conftest.c -o conftest.o >/dev/null 2>&1 &&
       grep conftest.h conftest.Po > /dev/null 2>&1; then
      am_cv_[$1]_dependencies_compiler_type="$depmode"
      break
    fi
  done

  rm -f conftest.*
else
  am_cv_[$1]_dependencies_compiler_type=none
fi
])
AC_MSG_RESULT($am_cv_[$1]_dependencies_compiler_type)
[$1]DEPMODE="depmode=$am_cv_[$1]_dependencies_compiler_type"
AC_SUBST([$1]DEPMODE)
])

# Choose a directory name for dependency files.
# This macro is AC_REQUIREd in AM_DEPENDENCIES

AC_DEFUN([AM_SET_DEPDIR],[
if test -d .deps || mkdir .deps 2> /dev/null || test -d .deps; then
  DEPDIR=.deps
  # We redirect because .deps might already exist and be populated.
  # In this situation we don't want to see an error.
  rmdir .deps > /dev/null 2>&1
else
  DEPDIR=_deps
fi
AC_SUBST(DEPDIR)
])

AC_DEFUN([AM_DEP_TRACK],[
AC_ARG_ENABLE(dependency-tracking,
[  --disable-dependency-tracking Speeds up one-time builds
  --enable-dependency-tracking  Do not reject slow dependency extractors])
if test "x$enable_dependency_tracking" = xno; then
  AMDEP="#"
else
  am_depcomp="$ac_aux_dir/depcomp"
  if test ! -f "$am_depcomp"; then
    AMDEP="#"
  else
    AMDEP=
  fi
fi
AC_SUBST(AMDEP)
if test -z "$AMDEP"; then
  AMDEPBACKSLASH='\'
else
  AMDEPBACKSLASH=
fi
pushdef([subst], defn([AC_SUBST]))
subst(AMDEPBACKSLASH)
popdef([subst])
])

# Generate code to set up dependency tracking.
# This macro should only be invoked once -- use via AC_REQUIRE.
# Usage:
# AM_OUTPUT_DEPENDENCY_COMMANDS

#
# This code is only required when automatic dependency tracking
# is enabled.  FIXME.  This creates each `.P' file that we will
# need in order to bootstrap the dependency handling code.
AC_DEFUN([AM_OUTPUT_DEPENDENCY_COMMANDS],[
AC_OUTPUT_COMMANDS([
test x"$AMDEP" != x"" ||
for mf in $CONFIG_FILES; do
  case "$mf" in
  Makefile) dirpart=.;;
  */Makefile) dirpart=`echo "$mf" | sed -e 's|/[^/]*$||'`;;
  *) continue;;
  esac
  grep '^DEP_FILES *= *[^ #]' < "$mf" > /dev/null || continue
  # Extract the definition of DEP_FILES from the Makefile without
  # running `make'.
  DEPDIR=`sed -n -e '/^DEPDIR = / s///p' < "$mf"`
  test -z "$DEPDIR" && continue
  # When using ansi2knr, U may be empty or an underscore; expand it
  U=`sed -n -e '/^U = / s///p' < "$mf"`
  test -d "$dirpart/$DEPDIR" || mkdir "$dirpart/$DEPDIR"
  # We invoke sed twice because it is the simplest approach to
  # changing $(DEPDIR) to its actual value in the expansion.
  for file in `sed -n -e '
    /^DEP_FILES = .*\\\\$/ {
      s/^DEP_FILES = //
      :loop
	s/\\\\$//
	p
	n
	/\\\\$/ b loop
      p
    }
    /^DEP_FILES = / s/^DEP_FILES = //p' < "$mf" | \
       sed -e 's/\$(DEPDIR)/'"$DEPDIR"'/g' -e 's/\$U/'"$U"'/g'`; do
    # Make sure the directory exists.
    test -f "$dirpart/$file" && continue
    fdir=`echo "$file" | sed -e 's|/[^/]*$||'`
    $ac_aux_dir/mkinstalldirs "$dirpart/$fdir" > /dev/null 2>&1
    # echo "creating $dirpart/$file"
    echo '# dummy' > "$dirpart/$file"
  done
done
], [AMDEP="$AMDEP"
ac_aux_dir="$ac_aux_dir"])])


# AM_PROG_LEX
# Look for flex, lex or missing, then run AC_PROG_LEX and AC_DECL_YYTEXT
AC_DEFUN([AM_PROG_LEX],
[AC_REQUIRE([AM_MISSING_HAS_RUN])
AC_CHECK_PROGS(LEX, flex lex, [${am_missing_run}flex])
AC_PROG_LEX
AC_DECL_YYTEXT])

dnl $KTH: krb-prog-ln-s.m4,v 1.1 1997/12/14 15:59:01 joda Exp $
dnl
dnl
dnl Better test for ln -s, ln or cp
dnl

AC_DEFUN(AC_KRB_PROG_LN_S,
[AC_MSG_CHECKING(for ln -s or something else)
AC_CACHE_VAL(ac_cv_prog_LN_S,
[rm -f conftestdata
if ln -s X conftestdata 2>/dev/null
then
  rm -f conftestdata
  ac_cv_prog_LN_S="ln -s"
else
  touch conftestdata1
  if ln conftestdata1 conftestdata2; then
    rm -f conftestdata*
    ac_cv_prog_LN_S=ln
  else
    ac_cv_prog_LN_S=cp
  fi
fi])dnl
LN_S="$ac_cv_prog_LN_S"
AC_MSG_RESULT($ac_cv_prog_LN_S)
AC_SUBST(LN_S)dnl
])


dnl $KTH: mips-abi.m4,v 1.5 2000/07/18 15:01:42 joda Exp $
dnl
dnl
dnl Check for MIPS/IRIX ABI flags. Sets $abi and $abilibdirext to some
dnl value.

AC_DEFUN(AC_MIPS_ABI, [
AC_ARG_WITH(mips_abi,
[  --with-mips-abi=abi     ABI to use for IRIX (32, n32, or 64)])

case "$host_os" in
irix*)
with_mips_abi="${with_mips_abi:-yes}"
if test -n "$GCC"; then

# GCC < 2.8 only supports the O32 ABI. GCC >= 2.8 has a flag to select
# which ABI to use, but only supports (as of 2.8.1) the N32 and 64 ABIs.
#
# Default to N32, but if GCC doesn't grok -mabi=n32, we assume an old
# GCC and revert back to O32. The same goes if O32 is asked for - old
# GCCs doesn't like the -mabi option, and new GCCs can't output O32.
#
# Don't you just love *all* the different SGI ABIs?

case "${with_mips_abi}" in 
        32|o32) abi='-mabi=32';  abilibdirext=''     ;;
       n32|yes) abi='-mabi=n32'; abilibdirext='32'  ;;
        64) abi='-mabi=64';  abilibdirext='64'   ;;
	no) abi=''; abilibdirext='';;
         *) AC_MSG_ERROR("Invalid ABI specified") ;;
esac
if test -n "$abi" ; then
ac_foo=krb_cv_gcc_`echo $abi | tr =- __`
dnl
dnl can't use AC_CACHE_CHECK here, since it doesn't quote CACHE-ID to
dnl AC_MSG_RESULT
dnl
AC_MSG_CHECKING([if $CC supports the $abi option])
AC_CACHE_VAL($ac_foo, [
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $abi"
AC_TRY_COMPILE(,int x;, eval $ac_foo=yes, eval $ac_foo=no)
CFLAGS="$save_CFLAGS"
])
ac_res=`eval echo \\\$$ac_foo`
AC_MSG_RESULT($ac_res)
if test $ac_res = no; then
# Try to figure out why that failed...
case $abi in
	-mabi=32) 
	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -mabi=n32"
	AC_TRY_COMPILE(,int x;, ac_res=yes, ac_res=no)
	CLAGS="$save_CFLAGS"
	if test $ac_res = yes; then
		# New GCC
		AC_MSG_ERROR([$CC does not support the $with_mips_abi ABI])
	fi
	# Old GCC
	abi=''
	abilibdirext=''
	;;
	-mabi=n32|-mabi=64)
		if test $with_mips_abi = yes; then
			# Old GCC, default to O32
			abi=''
			abilibdirext=''
		else
			# Some broken GCC
			AC_MSG_ERROR([$CC does not support the $with_mips_abi ABI])
		fi
	;;
esac
fi #if test $ac_res = no; then
fi #if test -n "$abi" ; then
else
case "${with_mips_abi}" in
        32|o32) abi='-32'; abilibdirext=''     ;;
       n32|yes) abi='-n32'; abilibdirext='32'  ;;
        64) abi='-64'; abilibdirext='64'   ;;
	no) abi=''; abilibdirext='';;
         *) AC_MSG_ERROR("Invalid ABI specified") ;;
esac
fi #if test -n "$GCC"; then
;;
esac
])

dnl
dnl $KTH: c-attribute.m4,v 1.2 1999/03/01 09:52:23 joda Exp $
dnl

dnl
dnl Test for __attribute__
dnl

AC_DEFUN(AC_C___ATTRIBUTE__, [
AC_MSG_CHECKING(for __attribute__)
AC_CACHE_VAL(ac_cv___attribute__, [
AC_TRY_COMPILE([
#include <stdlib.h>
],
[
static void foo(void) __attribute__ ((noreturn));

static void
foo(void)
{
  exit(1);
}
],
ac_cv___attribute__=yes,
ac_cv___attribute__=no)])
if test "$ac_cv___attribute__" = "yes"; then
  AC_DEFINE(HAVE___ATTRIBUTE__, 1, [define if your compiler has __attribute__])
fi
AC_MSG_RESULT($ac_cv___attribute__)
])


# libtool.m4 - Configure libtool for the host system. -*-Shell-script-*-

# serial 46 AC_PROG_LIBTOOL
AC_DEFUN([AC_PROG_LIBTOOL],
[AC_REQUIRE([AC_LIBTOOL_SETUP])dnl

# This can be used to rebuild libtool when needed
LIBTOOL_DEPS="$ac_aux_dir/ltmain.sh"

# Always use our own libtool.
LIBTOOL='$(SHELL) $(top_builddir)/libtool'
AC_SUBST(LIBTOOL)dnl

# Prevent multiple expansion
define([AC_PROG_LIBTOOL], [])
])

AC_DEFUN([AC_LIBTOOL_SETUP],
[AC_PREREQ(2.13)dnl
AC_REQUIRE([AC_ENABLE_SHARED])dnl
AC_REQUIRE([AC_ENABLE_STATIC])dnl
AC_REQUIRE([AC_ENABLE_FAST_INSTALL])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_PROG_LD])dnl
AC_REQUIRE([AC_PROG_LD_RELOAD_FLAG])dnl
AC_REQUIRE([AC_PROG_NM])dnl
AC_REQUIRE([AC_PROG_LN_S])dnl
AC_REQUIRE([AC_DEPLIBS_CHECK_METHOD])dnl
AC_REQUIRE([AC_OBJEXT])dnl
AC_REQUIRE([AC_EXEEXT])dnl
dnl

_LT_AC_PROG_ECHO_BACKSLASH
# Only perform the check for file, if the check method requires it
case $deplibs_check_method in
file_magic*)
  if test "$file_magic_cmd" = '$MAGIC_CMD'; then
    AC_PATH_MAGIC
  fi
  ;;
esac

AC_CHECK_TOOL(RANLIB, ranlib, :)
AC_CHECK_TOOL(STRIP, strip, :)

ifdef([AC_PROVIDE_AC_LIBTOOL_DLOPEN], enable_dlopen=yes, enable_dlopen=no)
ifdef([AC_PROVIDE_AC_LIBTOOL_WIN32_DLL],
enable_win32_dll=yes, enable_win32_dll=no)

AC_ARG_ENABLE(libtool-lock,
  [  --disable-libtool-lock  avoid locking (might break parallel builds)])
test "x$enable_libtool_lock" != xno && enable_libtool_lock=yes

# Some flags need to be propagated to the compiler or linker for good
# libtool support.
case $host in
*-*-irix6*)
  # Find out which ABI we are using.
  echo '[#]line __oline__ "configure"' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    case `/usr/bin/file conftest.$ac_objext` in
    *32-bit*)
      LD="${LD-ld} -32"
      ;;
    *N32*)
      LD="${LD-ld} -n32"
      ;;
    *64-bit*)
      LD="${LD-ld} -64"
      ;;
    esac
  fi
  rm -rf conftest*
  ;;

*-*-sco3.2v5*)
  # On SCO OpenServer 5, we need -belf to get full-featured binaries.
  SAVE_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -belf"
  AC_CACHE_CHECK([whether the C compiler needs -belf], lt_cv_cc_needs_belf,
    [AC_LANG_SAVE
     AC_LANG_C
     AC_TRY_LINK([],[],[lt_cv_cc_needs_belf=yes],[lt_cv_cc_needs_belf=no])
     AC_LANG_RESTORE])
  if test x"$lt_cv_cc_needs_belf" != x"yes"; then
    # this is probably gcc 2.8.0, egcs 1.0 or newer; no need for -belf
    CFLAGS="$SAVE_CFLAGS"
  fi
  ;;

ifdef([AC_PROVIDE_AC_LIBTOOL_WIN32_DLL],
[*-*-cygwin* | *-*-mingw* | *-*-pw32*)
  AC_CHECK_TOOL(DLLTOOL, dlltool, false)
  AC_CHECK_TOOL(AS, as, false)
  AC_CHECK_TOOL(OBJDUMP, objdump, false)

  # recent cygwin and mingw systems supply a stub DllMain which the user
  # can override, but on older systems we have to supply one
  AC_CACHE_CHECK([if libtool should supply DllMain function], lt_cv_need_dllmain,
    [AC_TRY_LINK([],
      [extern int __attribute__((__stdcall__)) DllMain(void*, int, void*);
      DllMain (0, 0, 0);],
      [lt_cv_need_dllmain=no],[lt_cv_need_dllmain=yes])])

  case $host/$CC in
  *-*-cygwin*/gcc*-mno-cygwin*|*-*-mingw*)
    # old mingw systems require "-dll" to link a DLL, while more recent ones
    # require "-mdll"
    SAVE_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -mdll"
    AC_CACHE_CHECK([how to link DLLs], lt_cv_cc_dll_switch,
      [AC_TRY_LINK([], [], [lt_cv_cc_dll_switch=-mdll],[lt_cv_cc_dll_switch=-dll])])
    CFLAGS="$SAVE_CFLAGS" ;;
  *-*-cygwin* | *-*-pw32*)
    # cygwin systems need to pass --dll to the linker, and not link
    # crt.o which will require a WinMain@16 definition.
    lt_cv_cc_dll_switch="-Wl,--dll -nostartfiles" ;;
  esac
  ;;
  ])
esac

_LT_AC_LTCONFIG_HACK

])

# _LT_AC_CHECK_DLFCN
# --------------------
AC_DEFUN(_LT_AC_CHECK_DLFCN,
[AC_CHECK_HEADERS(dlfcn.h)
])# _LT_AC_CHECK_DLFCN

# AC_LIBTOOL_SYS_GLOBAL_SYMBOL_PIPE
# ---------------------------------
AC_DEFUN([AC_LIBTOOL_SYS_GLOBAL_SYMBOL_PIPE],
[AC_REQUIRE([AC_CANONICAL_HOST])
AC_REQUIRE([AC_PROG_NM])
AC_REQUIRE([AC_OBJEXT])
# Check for command to grab the raw symbol name followed by C symbol from nm.
AC_MSG_CHECKING([command to parse $NM output])
AC_CACHE_VAL([lt_cv_sys_global_symbol_pipe], [dnl

# These are sane defaults that work on at least a few old systems.
# [They come from Ultrix.  What could be older than Ultrix?!! ;)]

# Character class describing NM global symbol codes.
[symcode='[BCDEGRST]']

# Regexp to match symbols that can be accessed directly from C.
[sympat='\([_A-Za-z][_A-Za-z0-9]*\)']

# Transform the above into a raw symbol and a C symbol.
symxfrm='\1 \2\3 \3'

# Transform an extracted symbol line into a proper C declaration
lt_cv_global_symbol_to_cdecl="sed -n -e 's/^. .* \(.*\)$/extern char \1;/p'"

# Define system-specific variables.
case $host_os in
aix*)
  [symcode='[BCDT]']
  ;;
cygwin* | mingw* | pw32*)
  [symcode='[ABCDGISTW]']
  ;;
hpux*) # Its linker distinguishes data from code symbols
  lt_cv_global_symbol_to_cdecl="sed -n -e 's/^T .* \(.*\)$/extern char \1();/p' -e 's/^$symcode* .* \(.*\)$/extern char \1;/p'"
  ;;
irix*)
  [symcode='[BCDEGRST]']
  ;;
solaris* | sysv5*)
  [symcode='[BDT]']
  ;;
sysv4)
  [symcode='[DFNSTU]']
  ;;
esac

# Handle CRLF in mingw tool chain
opt_cr=
case $host_os in
mingw*)
  opt_cr=`echo 'x\{0,1\}' | tr x '\015'` # option cr in regexp
  ;;
esac

# If we're using GNU nm, then use its standard symbol codes.
if $NM -V 2>&1 | egrep '(GNU|with BFD)' > /dev/null; then
  [symcode='[ABCDGISTW]']
fi

# Try without a prefix undercore, then with it.
for ac_symprfx in "" "_"; do

  # Write the raw and C identifiers.
[lt_cv_sys_global_symbol_pipe="sed -n -e 's/^.*[ 	]\($symcode$symcode*\)[ 	][ 	]*\($ac_symprfx\)$sympat$opt_cr$/$symxfrm/p'"]

  # Check to see that the pipe works correctly.
  pipe_works=no
  rm -f conftest*
  cat > conftest.$ac_ext <<EOF
#ifdef __cplusplus
extern "C" {
#endif
char nm_test_var;
void nm_test_func(){}
#ifdef __cplusplus
}
#endif
int main(){nm_test_var='a';nm_test_func();return(0);}
EOF

  if AC_TRY_EVAL(ac_compile); then
    # Now try to grab the symbols.
    nlist=conftest.nm
    if AC_TRY_EVAL(NM conftest.$ac_objext \| $lt_cv_sys_global_symbol_pipe \> $nlist) && test -s "$nlist"; then
      # Try sorting and uniquifying the output.
      if sort "$nlist" | uniq > "$nlist"T; then
	mv -f "$nlist"T "$nlist"
      else
	rm -f "$nlist"T
      fi

      # Make sure that we snagged all the symbols we need.
      if egrep ' nm_test_var$' "$nlist" >/dev/null; then
	if egrep ' nm_test_func$' "$nlist" >/dev/null; then
	  cat <<EOF > conftest.$ac_ext
#ifdef __cplusplus
extern "C" {
#endif

EOF
	  # Now generate the symbol file.
	  eval "$lt_cv_global_symbol_to_cdecl"' < "$nlist" >> conftest.$ac_ext'

	  cat <<EOF >> conftest.$ac_ext
#if defined (__STDC__) && __STDC__
# define lt_ptr_t void *
#else
# define lt_ptr_t char *
# define const
#endif

/* The mapping between symbol names and symbols. */
const struct {
  const char *name;
  lt_ptr_t address;
}
[lt_preloaded_symbols[] =]
{
EOF
	  sed "s/^$symcode$symcode* \(.*\) \(.*\)$/  {\"\2\", (lt_ptr_t) \&\2},/" < "$nlist" >> conftest.$ac_ext
	  cat <<\EOF >> conftest.$ac_ext
  {0, (lt_ptr_t) 0}
};

#ifdef __cplusplus
}
#endif
EOF
	  # Now try linking the two files.
	  mv conftest.$ac_objext conftstm.$ac_objext
	  save_LIBS="$LIBS"
	  save_CFLAGS="$CFLAGS"
	  LIBS="conftstm.$ac_objext"
	  CFLAGS="$CFLAGS$no_builtin_flag"
	  if AC_TRY_EVAL(ac_link) && test -s conftest; then
	    pipe_works=yes
	  fi
	  LIBS="$save_LIBS"
	  CFLAGS="$save_CFLAGS"
	else
	  echo "cannot find nm_test_func in $nlist" >&AC_FD_CC
	fi
      else
	echo "cannot find nm_test_var in $nlist" >&AC_FD_CC
      fi
    else
      echo "cannot run $lt_cv_sys_global_symbol_pipe" >&AC_FD_CC
    fi
  else
    echo "$progname: failed program was:" >&AC_FD_CC
    cat conftest.$ac_ext >&5
  fi
  rm -f conftest* conftst*

  # Do not use the global_symbol_pipe unless it works.
  if test "$pipe_works" = yes; then
    break
  else
    lt_cv_sys_global_symbol_pipe=
  fi
done
])
global_symbol_pipe="$lt_cv_sys_global_symbol_pipe"
if test -z "$lt_cv_sys_global_symbol_pipe"; then
  global_symbol_to_cdecl=
else
  global_symbol_to_cdecl="$lt_cv_global_symbol_to_cdecl"
fi
if test -z "$global_symbol_pipe$global_symbol_to_cdecl"; then
  AC_MSG_RESULT(failed)
else
  AC_MSG_RESULT(ok)
fi
]) # AC_LIBTOOL_SYS_GLOBAL_SYMBOL_PIPE

# _LT_AC_LIBTOOL_SYS_PATH_SEPARATOR
# ---------------------------------
AC_DEFUN([_LT_AC_LIBTOOL_SYS_PATH_SEPARATOR],
[# Find the correct PATH separator.  Usually this is `:', but
# DJGPP uses `;' like DOS.
if test "X${PATH_SEPARATOR+set}" != Xset; then
  UNAME=${UNAME-`uname 2>/dev/null`}
  case X$UNAME in
    *-DOS) lt_cv_sys_path_separator=';' ;;
    *)     lt_cv_sys_path_separator=':' ;;
  esac
fi
])# _LT_AC_LIBTOOL_SYS_PATH_SEPARATOR

# _LT_AC_PROG_ECHO_BACKSLASH
# --------------------------
# Add some code to the start of the generated configure script which
# will find an echo command which doesn;t interpret backslashes.
AC_DEFUN([_LT_AC_PROG_ECHO_BACKSLASH],
[ifdef([AC_DIVERSION_NOTICE], [AC_DIVERT_PUSH(AC_DIVERSION_NOTICE)],
                              [AC_DIVERT_PUSH(NOTICE)])
_LT_AC_LIBTOOL_SYS_PATH_SEPARATOR

# Check that we are running under the correct shell.
SHELL=${CONFIG_SHELL-/bin/sh}

case X$ECHO in
X*--fallback-echo)
  # Remove one level of quotation (which was required for Make).
  ECHO=`echo "$ECHO" | sed 's,\\\\\[$]\\[$]0,'[$]0','`
  ;;
esac

echo=${ECHO-echo}
if test "X[$]1" = X--no-reexec; then
  # Discard the --no-reexec flag, and continue.
  shift
elif test "X[$]1" = X--fallback-echo; then
  # Avoid inline document here, it may be left over
  :
elif test "X`($echo '\t') 2>/dev/null`" = 'X\t'; then
  # Yippee, $echo works!
  :
else
  # Restart under the correct shell.
  exec $SHELL "[$]0" --no-reexec ${1+"[$]@"}
fi

if test "X[$]1" = X--fallback-echo; then
  # used as fallback echo
  shift
  cat <<EOF
$*
EOF
  exit 0
fi

# The HP-UX ksh and POSIX shell print the target directory to stdout
# if CDPATH is set.
if test "X${CDPATH+set}" = Xset; then CDPATH=:; export CDPATH; fi

if test -z "$ECHO"; then
if test "X${echo_test_string+set}" != Xset; then
# find a string as large as possible, as long as the shell can cope with it
  for cmd in 'sed 50q "[$]0"' 'sed 20q "[$]0"' 'sed 10q "[$]0"' 'sed 2q "[$]0"' 'echo test'; do
    # expected sizes: less than 2Kb, 1Kb, 512 bytes, 16 bytes, ...
    if (echo_test_string="`eval $cmd`") 2>/dev/null &&
       echo_test_string="`eval $cmd`" &&
       (test "X$echo_test_string" = "X$echo_test_string") 2>/dev/null
    then
      break
    fi
  done
fi

if test "X`($echo '\t') 2>/dev/null`" = 'X\t' &&
   echo_testing_string=`($echo "$echo_test_string") 2>/dev/null` &&
   test "X$echo_testing_string" = "X$echo_test_string"; then
  :
else
  # The Solaris, AIX, and Digital Unix default echo programs unquote
  # backslashes.  This makes it impossible to quote backslashes using
  #   echo "$something" | sed 's/\\/\\\\/g'
  #
  # So, first we look for a working echo in the user's PATH.

  IFS="${IFS= 	}"; save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR}"
  for dir in $PATH /usr/ucb; do
    if (test -f $dir/echo || test -f $dir/echo$ac_exeext) &&
       test "X`($dir/echo '\t') 2>/dev/null`" = 'X\t' &&
       echo_testing_string=`($dir/echo "$echo_test_string") 2>/dev/null` &&
       test "X$echo_testing_string" = "X$echo_test_string"; then
      echo="$dir/echo"
      break
    fi
  done
  IFS="$save_ifs"

  if test "X$echo" = Xecho; then
    # We didn't find a better echo, so look for alternatives.
    if test "X`(print -r '\t') 2>/dev/null`" = 'X\t' &&
       echo_testing_string=`(print -r "$echo_test_string") 2>/dev/null` &&
       test "X$echo_testing_string" = "X$echo_test_string"; then
      # This shell has a builtin print -r that does the trick.
      echo='print -r'
    elif (test -f /bin/ksh || test -f /bin/ksh$ac_exeext) &&
	 test "X$CONFIG_SHELL" != X/bin/ksh; then
      # If we have ksh, try running configure again with it.
      ORIGINAL_CONFIG_SHELL=${CONFIG_SHELL-/bin/sh}
      export ORIGINAL_CONFIG_SHELL
      CONFIG_SHELL=/bin/ksh
      export CONFIG_SHELL
      exec $CONFIG_SHELL "[$]0" --no-reexec ${1+"[$]@"}
    else
      # Try using printf.
      echo='printf %s\n'
      if test "X`($echo '\t') 2>/dev/null`" = 'X\t' &&
	 echo_testing_string=`($echo "$echo_test_string") 2>/dev/null` &&
	 test "X$echo_testing_string" = "X$echo_test_string"; then
	# Cool, printf works
	:
      elif echo_testing_string=`($ORIGINAL_CONFIG_SHELL "[$]0" --fallback-echo '\t') 2>/dev/null` &&
	   test "X$echo_testing_string" = 'X\t' &&
	   echo_testing_string=`($ORIGINAL_CONFIG_SHELL "[$]0" --fallback-echo "$echo_test_string") 2>/dev/null` &&
	   test "X$echo_testing_string" = "X$echo_test_string"; then
	CONFIG_SHELL=$ORIGINAL_CONFIG_SHELL
	export CONFIG_SHELL
	SHELL="$CONFIG_SHELL"
	export SHELL
	echo="$CONFIG_SHELL [$]0 --fallback-echo"
      elif echo_testing_string=`($CONFIG_SHELL "[$]0" --fallback-echo '\t') 2>/dev/null` &&
	   test "X$echo_testing_string" = 'X\t' &&
	   echo_testing_string=`($CONFIG_SHELL "[$]0" --fallback-echo "$echo_test_string") 2>/dev/null` &&
	   test "X$echo_testing_string" = "X$echo_test_string"; then
	echo="$CONFIG_SHELL [$]0 --fallback-echo"
      else
	# maybe with a smaller string...
	prev=:

	for cmd in 'echo test' 'sed 2q "[$]0"' 'sed 10q "[$]0"' 'sed 20q "[$]0"' 'sed 50q "[$]0"'; do
	  if (test "X$echo_test_string" = "X`eval $cmd`") 2>/dev/null
	  then
	    break
	  fi
	  prev="$cmd"
	done

	if test "$prev" != 'sed 50q "[$]0"'; then
	  echo_test_string=`eval $prev`
	  export echo_test_string
	  exec ${ORIGINAL_CONFIG_SHELL-${CONFIG_SHELL-/bin/sh}} "[$]0" ${1+"[$]@"}
	else
	  # Oops.  We lost completely, so just stick with echo.
	  echo=echo
	fi
      fi
    fi
  fi
fi
fi

# Copy echo and quote the copy suitably for passing to libtool from
# the Makefile, instead of quoting the original, which is used later.
ECHO=$echo
if test "X$ECHO" = "X$CONFIG_SHELL [$]0 --fallback-echo"; then
   ECHO="$CONFIG_SHELL \\\$\[$]0 --fallback-echo"
fi

AC_SUBST(ECHO)
AC_DIVERT_POP
])# _LT_AC_PROG_ECHO_BACKSLASH

# _LT_AC_TRY_DLOPEN_SELF (ACTION-IF-TRUE, ACTION-IF-TRUE-W-USCORE,
#                           ACTION-IF-FALSE, ACTION-IF-CROSS-COMPILING)
# ------------------------------------------------------------------
AC_DEFUN(_LT_AC_TRY_DLOPEN_SELF,
[if test "$cross_compiling" = yes; then :
  [$4]
else
  AC_REQUIRE([_LT_AC_CHECK_DLFCN])dnl
  lt_dlunknown=0; lt_dlno_uscore=1; lt_dlneed_uscore=2
  lt_status=$lt_dlunknown
  cat > conftest.$ac_ext <<EOF
[#line __oline__ "configure"
#include "confdefs.h"

#if HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#include <stdio.h>

#ifdef RTLD_GLOBAL
#  define LT_DLGLOBAL		RTLD_GLOBAL
#else
#  ifdef DL_GLOBAL
#    define LT_DLGLOBAL		DL_GLOBAL
#  else
#    define LT_DLGLOBAL		0
#  endif
#endif

/* We may have to define LT_DLLAZY_OR_NOW in the command line if we
   find out it does not work in some platform. */
#ifndef LT_DLLAZY_OR_NOW
#  ifdef RTLD_LAZY
#    define LT_DLLAZY_OR_NOW		RTLD_LAZY
#  else
#    ifdef DL_LAZY
#      define LT_DLLAZY_OR_NOW		DL_LAZY
#    else
#      ifdef RTLD_NOW
#        define LT_DLLAZY_OR_NOW	RTLD_NOW
#      else
#        ifdef DL_NOW
#          define LT_DLLAZY_OR_NOW	DL_NOW
#        else
#          define LT_DLLAZY_OR_NOW	0
#        endif
#      endif
#    endif
#  endif
#endif

#ifdef __cplusplus
extern "C" void exit (int);
#endif

void fnord() { int i=42;}
int main ()
{
  void *self = dlopen (0, LT_DLGLOBAL|LT_DLLAZY_OR_NOW);
  int status = $lt_dlunknown;

  if (self)
    {
      if (dlsym (self,"fnord"))       status = $lt_dlno_uscore;
      else if (dlsym( self,"_fnord")) status = $lt_dlneed_uscore;
      /* dlclose (self); */
    }

    exit (status);
}]
EOF
  if AC_TRY_EVAL(ac_link) && test -s conftest${ac_exeext} 2>/dev/null; then
    (./conftest; exit; ) 2>/dev/null
    lt_status=$?
    case x$lt_status in
      x$lt_dlno_uscore) $1 ;;
      x$lt_dlneed_uscore) $2 ;;
      x$lt_unknown|x*) $3 ;;
    esac
  else :
    # compilation failed
    $3
  fi
fi
rm -fr conftest*
])# _LT_AC_TRY_DLOPEN_SELF

# AC_LIBTOOL_DLOPEN_SELF
# -------------------
AC_DEFUN(AC_LIBTOOL_DLOPEN_SELF,
[if test "x$enable_dlopen" != xyes; then
  enable_dlopen=unknown
  enable_dlopen_self=unknown
  enable_dlopen_self_static=unknown
else
  lt_cv_dlopen=no
  lt_cv_dlopen_libs=

  case $host_os in
  beos*)
    lt_cv_dlopen="load_add_on"
    lt_cv_dlopen_libs=
    lt_cv_dlopen_self=yes
    ;;

  cygwin* | mingw* | pw32*)
    lt_cv_dlopen="LoadLibrary"
    lt_cv_dlopen_libs=
   ;;

  *)
    AC_CHECK_LIB(dl, dlopen,  [lt_cv_dlopen="dlopen" lt_cv_dlopen_libs="-ldl"],
      [AC_CHECK_FUNC(dlopen, lt_cv_dlopen="dlopen",
        [AC_CHECK_FUNC(shl_load, lt_cv_dlopen="shl_load",
          [AC_CHECK_LIB(svld, dlopen,
	    [lt_cv_dlopen="dlopen" lt_cv_dlopen_libs="-lsvld"],
            [AC_CHECK_LIB(dld, shl_load,
              [lt_cv_dlopen="dld_link" lt_cv_dlopen_libs="-dld"])
	    ])
          ])
        ])
      ])
    ;;
  esac

  if test "x$lt_cv_dlopen" != xno; then
    enable_dlopen=yes
  else
    enable_dlopen=no
  fi

  case $lt_cv_dlopen in
  dlopen)
    save_CPPFLAGS="$CPPFLAGS"
    AC_REQUIRE([_LT_AC_CHECK_DLFCN])dnl
    test "x$ac_cv_header_dlfcn_h" = xyes && CPPFLAGS="$CPPFLAGS -DHAVE_DLFCN_H"

    save_LDFLAGS="$LDFLAGS"
    eval LDFLAGS=\"\$LDFLAGS $export_dynamic_flag_spec\"

    save_LIBS="$LIBS"
    LIBS="$lt_cv_dlopen_libs $LIBS"

    AC_CACHE_CHECK([whether a program can dlopen itself],
	  lt_cv_dlopen_self, [dnl
	  _LT_AC_TRY_DLOPEN_SELF(
	    lt_cv_dlopen_self=yes, lt_cv_dlopen_self=yes,
	    lt_cv_dlopen_self=no, lt_cv_dlopen_self=cross)
    ])

    if test "x$lt_cv_dlopen_self" = xyes; then
      LDFLAGS="$LDFLAGS $link_static_flag"
      AC_CACHE_CHECK([whether a statically linked program can dlopen itself],
    	  lt_cv_dlopen_self_static, [dnl
	  _LT_AC_TRY_DLOPEN_SELF(
	    lt_cv_dlopen_self_static=yes, lt_cv_dlopen_self_static=yes,
	    lt_cv_dlopen_self_static=no,  lt_cv_dlopen_self_static=cross)
      ])
    fi

    CPPFLAGS="$save_CPPFLAGS"
    LDFLAGS="$save_LDFLAGS"
    LIBS="$save_LIBS"
    ;;
  esac

  case $lt_cv_dlopen_self in
  yes|no) enable_dlopen_self=$lt_cv_dlopen_self ;;
  *) enable_dlopen_self=unknown ;;
  esac

  case $lt_cv_dlopen_self_static in
  yes|no) enable_dlopen_self_static=$lt_cv_dlopen_self_static ;;
  *) enable_dlopen_self_static=unknown ;;
  esac
fi
])# AC_LIBTOOL_DLOPEN_SELF

AC_DEFUN([_LT_AC_LTCONFIG_HACK],
[AC_REQUIRE([AC_LIBTOOL_SYS_GLOBAL_SYMBOL_PIPE])dnl
# Sed substitution that helps us do robust quoting.  It backslashifies
# metacharacters that are still active within double-quoted strings.
Xsed='sed -e s/^X//'
[sed_quote_subst='s/\([\\"\\`$\\\\]\)/\\\1/g']

# Same as above, but do not quote variable references.
[double_quote_subst='s/\([\\"\\`\\\\]\)/\\\1/g']

# Sed substitution to delay expansion of an escaped shell variable in a
# double_quote_subst'ed string.
delay_variable_subst='s/\\\\\\\\\\\$/\\\\\\$/g'

# Constants:
rm="rm -f"

# Global variables:
default_ofile=libtool
can_build_shared=yes

# All known linkers require a `.a' archive for static linking (except M$VC,
# which needs '.lib').
libext=a
ltmain="$ac_aux_dir/ltmain.sh"
ofile="$default_ofile"
with_gnu_ld="$lt_cv_prog_gnu_ld"
need_locks="$enable_libtool_lock"

old_CC="$CC"
old_CFLAGS="$CFLAGS"

# Set sane defaults for various variables
test -z "$AR" && AR=ar
test -z "$AR_FLAGS" && AR_FLAGS=cru
test -z "$AS" && AS=as
test -z "$CC" && CC=cc
test -z "$DLLTOOL" && DLLTOOL=dlltool
test -z "$LD" && LD=ld
test -z "$LN_S" && LN_S="ln -s"
test -z "$MAGIC_CMD" && MAGIC_CMD=file
test -z "$NM" && NM=nm
test -z "$OBJDUMP" && OBJDUMP=objdump
test -z "$RANLIB" && RANLIB=:
test -z "$STRIP" && STRIP=:
test -z "$ac_objext" && ac_objext=o

if test x"$host" != x"$build"; then
  ac_tool_prefix=${host_alias}-
else
  ac_tool_prefix=
fi

# Transform linux* to *-*-linux-gnu*, to support old configure scripts.
case $host_os in
linux-gnu*) ;;
linux*) host=`echo $host | sed 's/^\(.*-.*-linux\)\(.*\)$/\1-gnu\2/'`
esac

case $host_os in
aix3*)
  # AIX sometimes has problems with the GCC collect2 program.  For some
  # reason, if we set the COLLECT_NAMES environment variable, the problems
  # vanish in a puff of smoke.
  if test "X${COLLECT_NAMES+set}" != Xset; then
    COLLECT_NAMES=
    export COLLECT_NAMES
  fi
  ;;
esac

# Determine commands to create old-style static archives.
old_archive_cmds='$AR $AR_FLAGS $oldlib$oldobjs$old_deplibs'
old_postinstall_cmds='chmod 644 $oldlib'
old_postuninstall_cmds=

if test -n "$RANLIB"; then
  old_archive_cmds="$old_archive_cmds~\$RANLIB \$oldlib"
  old_postinstall_cmds="\$RANLIB \$oldlib~$old_postinstall_cmds"
fi

# Allow CC to be a program name with arguments.
set dummy $CC
compiler="[$]2"

AC_MSG_CHECKING([for objdir])
rm -f .libs 2>/dev/null
mkdir .libs 2>/dev/null
if test -d .libs; then
  objdir=.libs
else
  # MS-DOS does not allow filenames that begin with a dot.
  objdir=_libs
fi
rmdir .libs 2>/dev/null
AC_MSG_RESULT($objdir)


AC_ARG_WITH(pic, 
[  --with-pic              try to use only PIC/non-PIC objects [default=use both]],
pic_mode="$withval", pic_mode=default)
test -z "$pic_mode" && pic_mode=default

# We assume here that the value for lt_cv_prog_cc_pic will not be cached
# in isolation, and that seeing it set (from the cache) indicates that
# the associated values are set (in the cache) correctly too.
AC_MSG_CHECKING([for $compiler option to produce PIC])
AC_CACHE_VAL(lt_cv_prog_cc_pic,
[ lt_cv_prog_cc_pic=
  lt_cv_prog_cc_shlib=
  lt_cv_prog_cc_wl=
  lt_cv_prog_cc_static=
  lt_cv_prog_cc_no_builtin=
  lt_cv_prog_cc_can_build_shared=$can_build_shared

  if test "$GCC" = yes; then
    lt_cv_prog_cc_wl='-Wl,'
    lt_cv_prog_cc_static='-static'

    case $host_os in
    aix*)
      # Below there is a dirty hack to force normal static linking with -ldl
      # The problem is because libdl dynamically linked with both libc and
      # libC (AIX C++ library), which obviously doesn't included in libraries
      # list by gcc. This cause undefined symbols with -static flags.
      # This hack allows C programs to be linked with "-static -ldl", but
      # we not sure about C++ programs.
      lt_cv_prog_cc_static="$lt_cv_prog_cc_static ${lt_cv_prog_cc_wl}-lC"
      ;;
    amigaos*)
      # FIXME: we need at least 68020 code to build shared libraries, but
      # adding the `-m68020' flag to GCC prevents building anything better,
      # like `-m68040'.
      lt_cv_prog_cc_pic='-m68020 -resident32 -malways-restore-a4'
      ;;
    beos* | irix5* | irix6* | osf3* | osf4* | osf5*)
      # PIC is the default for these OSes.
      ;;
    darwin* | rhapsody*)
      # PIC is the default on this platform
      # Common symbols not allowed in MH_DYLIB files
      lt_cv_prog_cc_pic='-fno-common'
      ;;
    cygwin* | mingw* | pw32* | os2*)
      # This hack is so that the source file can tell whether it is being
      # built for inclusion in a dll (and should export symbols for example).
      lt_cv_prog_cc_pic='-DDLL_EXPORT'
      ;;
    sysv4*MP*)
      if test -d /usr/nec; then
	 lt_cv_prog_cc_pic=-Kconform_pic
      fi
      ;;
    *)
      lt_cv_prog_cc_pic='-fPIC'
      ;;
    esac
  else
    # PORTME Check for PIC flags for the system compiler.
    case $host_os in
    aix3* | aix4* | aix5*)
      # All AIX code is PIC.
      if test "$host_cpu" = ia64; then
        # AIX 5 now supports IA64 processor
        lt_cv_prog_cc_static='-Bstatic'
        lt_cv_prog_cc_wl='-Wl,'
      else
        lt_cv_prog_cc_static='-bnso -bI:/lib/syscalls.exp'
      fi
      ;;

    hpux9* | hpux10* | hpux11*)
      # Is there a better lt_cv_prog_cc_static that works with the bundled CC?
      lt_cv_prog_cc_wl='-Wl,'
      lt_cv_prog_cc_static="${lt_cv_prog_cc_wl}-a ${lt_cv_prog_cc_wl}archive"
      lt_cv_prog_cc_pic='+Z'
      ;;

    irix5* | irix6*)
      lt_cv_prog_cc_wl='-Wl,'
      lt_cv_prog_cc_static='-non_shared'
      # PIC (with -KPIC) is the default.
      ;;

    cygwin* | mingw* | pw32* | os2*)
      # This hack is so that the source file can tell whether it is being
      # built for inclusion in a dll (and should export symbols for example).
      lt_cv_prog_cc_pic='-DDLL_EXPORT'
      ;;

    newsos6)
      lt_cv_prog_cc_pic='-KPIC'
      lt_cv_prog_cc_static='-Bstatic'
      ;;

    osf3* | osf4* | osf5*)
      # All OSF/1 code is PIC.
      lt_cv_prog_cc_wl='-Wl,'
      lt_cv_prog_cc_static='-non_shared'
      ;;

    sco3.2v5*)
      lt_cv_prog_cc_pic='-Kpic'
      lt_cv_prog_cc_static='-dn'
      lt_cv_prog_cc_shlib='-belf'
      ;;

    solaris*)
      lt_cv_prog_cc_pic='-KPIC'
      lt_cv_prog_cc_static='-Bstatic'
      lt_cv_prog_cc_wl='-Wl,'
      ;;

    sunos4*)
      lt_cv_prog_cc_pic='-PIC'
      lt_cv_prog_cc_static='-Bstatic'
      lt_cv_prog_cc_wl='-Qoption ld '
      ;;

    sysv4 | sysv4.2uw2* | sysv4.3* | sysv5*)
      lt_cv_prog_cc_pic='-KPIC'
      lt_cv_prog_cc_static='-Bstatic'
      if test "x$host_vendor" = xsni; then
        lt_cv_prog_cc_wl='-LD'
      else
        lt_cv_prog_cc_wl='-Wl,'
      fi
      ;;

    uts4*)
      lt_cv_prog_cc_pic='-pic'
      lt_cv_prog_cc_static='-Bstatic'
      ;;

    sysv4*MP*)
      if test -d /usr/nec ;then
	lt_cv_prog_cc_pic='-Kconform_pic'
	lt_cv_prog_cc_static='-Bstatic'
      fi
      ;;

    *)
      lt_cv_prog_cc_can_build_shared=no
      ;;
    esac
  fi
])
if test -z "$lt_cv_prog_cc_pic"; then
  AC_MSG_RESULT([none])
else
  AC_MSG_RESULT([$lt_cv_prog_cc_pic])

  # Check to make sure the pic_flag actually works.
  AC_MSG_CHECKING([if $compiler PIC flag $lt_cv_prog_cc_pic works])
  AC_CACHE_VAL(lt_cv_prog_cc_pic_works, [dnl
    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $lt_cv_prog_cc_pic -DPIC"
    AC_TRY_COMPILE([], [], [dnl
      case $host_os in
      hpux9* | hpux10* | hpux11*)
	# On HP-UX, both CC and GCC only warn that PIC is supported... then
	# they create non-PIC objects.  So, if there were any warnings, we
	# assume that PIC is not supported.
	if test -s conftest.err; then
	  lt_cv_prog_cc_pic_works=no
	else
	  lt_cv_prog_cc_pic_works=yes
	fi
	;;
      *)
	lt_cv_prog_cc_pic_works=yes
	;;
      esac
    ], [dnl
      lt_cv_prog_cc_pic_works=no
    ])
    CFLAGS="$save_CFLAGS"
  ])

  if test "X$lt_cv_prog_cc_pic_works" = Xno; then
    lt_cv_prog_cc_pic=
    lt_cv_prog_cc_can_build_shared=no
  else
    lt_cv_prog_cc_pic=" $lt_cv_prog_cc_pic"
  fi

  AC_MSG_RESULT([$lt_cv_prog_cc_pic_works])
fi

# Check for any special shared library compilation flags.
if test -n "$lt_cv_prog_cc_shlib"; then
  AC_MSG_WARN([\`$CC' requires \`$lt_cv_prog_cc_shlib' to build shared libraries])
  if echo "$old_CC $old_CFLAGS " | [egrep -e "[ 	]$lt_cv_prog_cc_shlib[ 	]"] >/dev/null; then :
  else
   AC_MSG_WARN([add \`$lt_cv_prog_cc_shlib' to the CC or CFLAGS env variable and reconfigure])
    lt_cv_prog_cc_can_build_shared=no
  fi
fi

AC_MSG_CHECKING([if $compiler static flag $lt_cv_prog_cc_static works])
AC_CACHE_VAL([lt_cv_prog_cc_static_works], [dnl
  lt_cv_prog_cc_static_works=no
  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS $lt_cv_prog_cc_static"
  AC_TRY_LINK([], [], [lt_cv_prog_cc_static_works=yes])
  LDFLAGS="$save_LDFLAGS"
])

# Belt *and* braces to stop my trousers falling down:
test "X$lt_cv_prog_cc_static_works" = Xno && lt_cv_prog_cc_static=
AC_MSG_RESULT([$lt_cv_prog_cc_static_works])

pic_flag="$lt_cv_prog_cc_pic"
special_shlib_compile_flags="$lt_cv_prog_cc_shlib"
wl="$lt_cv_prog_cc_wl"
link_static_flag="$lt_cv_prog_cc_static"
no_builtin_flag="$lt_cv_prog_cc_no_builtin"
can_build_shared="$lt_cv_prog_cc_can_build_shared"


# Check to see if options -o and -c are simultaneously supported by compiler
AC_MSG_CHECKING([if $compiler supports -c -o file.$ac_objext])
AC_CACHE_VAL([lt_cv_compiler_c_o], [
$rm -r conftest 2>/dev/null
mkdir conftest
cd conftest
echo "int some_variable = 0;" > conftest.$ac_ext
mkdir out
# According to Tom Tromey, Ian Lance Taylor reported there are C compilers
# that will create temporary files in the current directory regardless of
# the output directory.  Thus, making CWD read-only will cause this test
# to fail, enabling locking or at least warning the user not to do parallel
# builds.
chmod -w .
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -o out/conftest2.$ac_objext"
compiler_c_o=no
if { (eval echo configure:__oline__: \"$ac_compile\") 1>&5; (eval $ac_compile) 2>out/conftest.err; } && test -s out/conftest2.$ac_objext; then
  # The compiler can only warn and ignore the option if not recognized
  # So say no if there are warnings
  if test -s out/conftest.err; then
    lt_cv_compiler_c_o=no
  else
    lt_cv_compiler_c_o=yes
  fi
else
  # Append any errors to the config.log.
  cat out/conftest.err 1>&AC_FD_CC
  lt_cv_compiler_c_o=no
fi
CFLAGS="$save_CFLAGS"
chmod u+w .
$rm conftest* out/*
rmdir out
cd ..
rmdir conftest
$rm -r conftest 2>/dev/null
])
compiler_c_o=$lt_cv_compiler_c_o
AC_MSG_RESULT([$compiler_c_o])

if test x"$compiler_c_o" = x"yes"; then
  # Check to see if we can write to a .lo
  AC_MSG_CHECKING([if $compiler supports -c -o file.lo])
  AC_CACHE_VAL([lt_cv_compiler_o_lo], [
  lt_cv_compiler_o_lo=no
  save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -c -o conftest.lo"
  AC_TRY_COMPILE([], [int some_variable = 0;], [dnl
    # The compiler can only warn and ignore the option if not recognized
    # So say no if there are warnings
    if test -s conftest.err; then
      lt_cv_compiler_o_lo=no
    else
      lt_cv_compiler_o_lo=yes
    fi
  ])
  CFLAGS="$save_CFLAGS"
  ])
  compiler_o_lo=$lt_cv_compiler_o_lo
  AC_MSG_RESULT([$compiler_c_lo])
else
  compiler_o_lo=no
fi

# Check to see if we can do hard links to lock some files if needed
hard_links="nottested"
if test "$compiler_c_o" = no && test "$need_locks" != no; then
  # do not overwrite the value of need_locks provided by the user
  AC_MSG_CHECKING([if we can lock with hard links])
  hard_links=yes
  $rm conftest*
  ln conftest.a conftest.b 2>/dev/null && hard_links=no
  touch conftest.a
  ln conftest.a conftest.b 2>&5 || hard_links=no
  ln conftest.a conftest.b 2>/dev/null && hard_links=no
  AC_MSG_RESULT([$hard_links])
  if test "$hard_links" = no; then
    AC_MSG_WARN([\`$CC' does not support \`-c -o', so \`make -j' may be unsafe])
    need_locks=warn
  fi
else
  need_locks=no
fi

if test "$GCC" = yes; then
  # Check to see if options -fno-rtti -fno-exceptions are supported by compiler
  AC_MSG_CHECKING([if $compiler supports -fno-rtti -fno-exceptions])
  echo "int some_variable = 0;" > conftest.$ac_ext
  save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -fno-rtti -fno-exceptions -c conftest.$ac_ext"
  compiler_rtti_exceptions=no
  AC_TRY_COMPILE([], [int some_variable = 0;], [dnl
    # The compiler can only warn and ignore the option if not recognized
    # So say no if there are warnings
    if test -s conftest.err; then
      compiler_rtti_exceptions=no
    else
      compiler_rtti_exceptions=yes
    fi
  ])
  CFLAGS="$save_CFLAGS"
  AC_MSG_RESULT([$compiler_rtti_exceptions])

  if test "$compiler_rtti_exceptions" = "yes"; then
    no_builtin_flag=' -fno-builtin -fno-rtti -fno-exceptions'
  else
    no_builtin_flag=' -fno-builtin'
  fi
fi

# See if the linker supports building shared libraries.
AC_MSG_CHECKING([whether the linker ($LD) supports shared libraries])

allow_undefined_flag=
no_undefined_flag=
need_lib_prefix=unknown
need_version=unknown
# when you set need_version to no, make sure it does not cause -set_version
# flags to be left without arguments
archive_cmds=
archive_expsym_cmds=
old_archive_from_new_cmds=
old_archive_from_expsyms_cmds=
export_dynamic_flag_spec=
whole_archive_flag_spec=
thread_safe_flag_spec=
hardcode_into_libs=no
hardcode_libdir_flag_spec=
hardcode_libdir_separator=
hardcode_direct=no
hardcode_minus_L=no
hardcode_shlibpath_var=unsupported
runpath_var=
link_all_deplibs=unknown
always_export_symbols=no
export_symbols_cmds='$NM $libobjs $convenience | $global_symbol_pipe | sed '\''s/.* //'\'' | sort | uniq > $export_symbols'
# include_expsyms should be a list of space-separated symbols to be *always*
# included in the symbol list
include_expsyms=
# exclude_expsyms can be an egrep regular expression of symbols to exclude
# it will be wrapped by ` (' and `)$', so one must not match beginning or
# end of line.  Example: `a|bc|.*d.*' will exclude the symbols `a' and `bc',
# as well as any symbol that contains `d'.
exclude_expsyms="_GLOBAL_OFFSET_TABLE_"
# Although _GLOBAL_OFFSET_TABLE_ is a valid symbol C name, most a.out
# platforms (ab)use it in PIC code, but their linkers get confused if
# the symbol is explicitly referenced.  Since portable code cannot
# rely on this symbol name, it's probably fine to never include it in
# preloaded symbol tables.
extract_expsyms_cmds=

case $host_os in
cygwin* | mingw* | pw32* )
  # FIXME: the MSVC++ port hasn't been tested in a loooong time
  # When not using gcc, we currently assume that we are using
  # Microsoft Visual C++.
  if test "$GCC" != yes; then
    with_gnu_ld=no
  fi
  ;;

esac

ld_shlibs=yes
if test "$with_gnu_ld" = yes; then
  # If archive_cmds runs LD, not CC, wlarc should be empty
  wlarc='${wl}'

  # See if GNU ld supports shared libraries.
  case $host_os in
  aix3* | aix4* | aix5*)
    # On AIX, the GNU linker is very broken
    # Note:Check GNU linker on AIX 5-IA64 when/if it becomes available.
    ld_shlibs=no
    cat <<EOF 1>&2

*** Warning: the GNU linker, at least up to release 2.9.1, is reported
*** to be unable to reliably create shared libraries on AIX.
*** Therefore, libtool is disabling shared libraries support.  If you
*** really care for shared libraries, you may want to modify your PATH
*** so that a non-GNU linker is found, and then restart.

EOF
    ;;

  amigaos*)
    archive_cmds='$rm $output_objdir/a2ixlibrary.data~$echo "#define NAME $libname" > $output_objdir/a2ixlibrary.data~$echo "#define LIBRARY_ID 1" >> $output_objdir/a2ixlibrary.data~$echo "#define VERSION $major" >> $output_objdir/a2ixlibrary.data~$echo "#define REVISION $revision" >> $output_objdir/a2ixlibrary.data~$AR $AR_FLAGS $lib $libobjs~$RANLIB $lib~(cd $output_objdir && a2ixlibrary -32)'
    hardcode_libdir_flag_spec='-L$libdir'
    hardcode_minus_L=yes

    # Samuel A. Falvo II <kc5tja@dolphin.openprojects.net> reports
    # that the semantics of dynamic libraries on AmigaOS, at least up
    # to version 4, is to share data among multiple programs linked
    # with the same dynamic library.  Since this doesn't match the
    # behavior of shared libraries on other platforms, we can use
    # them.
    ld_shlibs=no
    ;;

  beos*)
    if $LD --help 2>&1 | egrep ': supported targets:.* elf' > /dev/null; then
      allow_undefined_flag=unsupported
      # Joseph Beckenbach <jrb3@best.com> says some releases of gcc
      # support --undefined.  This deserves some investigation.  FIXME
      archive_cmds='$CC -nostart $libobjs $deplibs $compiler_flags ${wl}-soname $wl$soname -o $lib'
    else
      ld_shlibs=no
    fi
    ;;

  cygwin* | mingw* | pw32*)
    # hardcode_libdir_flag_spec is actually meaningless, as there is
    # no search path for DLLs.
    hardcode_libdir_flag_spec='-L$libdir'
    allow_undefined_flag=unsupported
    always_export_symbols=yes

    extract_expsyms_cmds='test -f $output_objdir/impgen.c || \
      sed -e "/^# \/\* impgen\.c starts here \*\//,/^# \/\* impgen.c ends here \*\// { s/^# //;s/^# *$//; p; }" -e d < $''0 > $output_objdir/impgen.c~
      test -f $output_objdir/impgen.exe || (cd $output_objdir && \
      if test "x$HOST_CC" != "x" ; then $HOST_CC -o impgen impgen.c ; \
      else $CC -o impgen impgen.c ; fi)~
      $output_objdir/impgen $dir/$soroot > $output_objdir/$soname-def'

    old_archive_from_expsyms_cmds='$DLLTOOL --as=$AS --dllname $soname --def $output_objdir/$soname-def --output-lib $output_objdir/$newlib'

    # cygwin and mingw dlls have different entry points and sets of symbols
    # to exclude.
    # FIXME: what about values for MSVC?
    dll_entry=__cygwin_dll_entry@12
    dll_exclude_symbols=DllMain@12,_cygwin_dll_entry@12,_cygwin_noncygwin_dll_entry@12~
    case $host_os in
    mingw*)
      # mingw values
      dll_entry=_DllMainCRTStartup@12
      dll_exclude_symbols=DllMain@12,DllMainCRTStartup@12,DllEntryPoint@12~
      ;;
    esac

    # mingw and cygwin differ, and it's simplest to just exclude the union
    # of the two symbol sets.
    dll_exclude_symbols=DllMain@12,_cygwin_dll_entry@12,_cygwin_noncygwin_dll_entry@12,DllMainCRTStartup@12,DllEntryPoint@12

    # recent cygwin and mingw systems supply a stub DllMain which the user
    # can override, but on older systems we have to supply one (in ltdll.c)
    if test "x$lt_cv_need_dllmain" = "xyes"; then
      ltdll_obj='$output_objdir/$soname-ltdll.'"$ac_objext "
      ltdll_cmds='test -f $output_objdir/$soname-ltdll.c || sed -e "/^# \/\* ltdll\.c starts here \*\//,/^# \/\* ltdll.c ends here \*\// { s/^# //; p; }" -e d < [$]0 > $output_objdir/$soname-ltdll.c~
	test -f $output_objdir/$soname-ltdll.$ac_objext || (cd $output_objdir && $CC -c $soname-ltdll.c)~'
    else
      ltdll_obj=
      ltdll_cmds=
    fi

    # Extract the symbol export list from an `--export-all' def file,
    # then regenerate the def file from the symbol export list, so that
    # the compiled dll only exports the symbol export list.
    # Be careful not to strip the DATA tag left be newer dlltools.
    export_symbols_cmds="$ltdll_cmds"'
      $DLLTOOL --export-all --exclude-symbols '$dll_exclude_symbols' --output-def $output_objdir/$soname-def '$ltdll_obj'$libobjs $convenience~
      [sed -e "1,/EXPORTS/d" -e "s/ @ [0-9]*//" -e "s/ *;.*$//"] < $output_objdir/$soname-def > $export_symbols'

    # If the export-symbols file already is a .def file (1st line
    # is EXPORTS), use it as is.
    # If DATA tags from a recent dlltool are present, honour them!
    archive_expsym_cmds='if test "x`head -1 $export_symbols`" = xEXPORTS; then
        cp $export_symbols $output_objdir/$soname-def;
      else
        echo EXPORTS > $output_objdir/$soname-def;
        _lt_hint=1;
        cat $export_symbols | while read symbol; do
         set dummy \$symbol;
         case \[$]# in
           2) echo "   \[$]2 @ \$_lt_hint ; " >> $output_objdir/$soname-def;;
           *) echo "     \[$]2 @ \$_lt_hint \[$]3 ; " >> $output_objdir/$soname-def;;
         esac;
         _lt_hint=`expr 1 + \$_lt_hint`;
        done;
      fi~
      '"$ltdll_cmds"'
      $CC -Wl,--base-file,$output_objdir/$soname-base '$lt_cv_cc_dll_switch' -Wl,-e,'$dll_entry' -o $output_objdir/$soname '$ltdll_obj'$libobjs $deplibs $compiler_flags~
      $DLLTOOL --as=$AS --dllname $soname --exclude-symbols '$dll_exclude_symbols' --def $output_objdir/$soname-def --base-file $output_objdir/$soname-base --output-exp $output_objdir/$soname-exp~
      $CC -Wl,--base-file,$output_objdir/$soname-base $output_objdir/$soname-exp '$lt_cv_cc_dll_switch' -Wl,-e,'$dll_entry' -o $output_objdir/$soname '$ltdll_obj'$libobjs $deplibs $compiler_flags~
      $DLLTOOL --as=$AS --dllname $soname --exclude-symbols '$dll_exclude_symbols' --def $output_objdir/$soname-def --base-file $output_objdir/$soname-base --output-exp $output_objdir/$soname-exp --output-lib $output_objdir/$libname.dll.a~
      $CC $output_objdir/$soname-exp '$lt_cv_cc_dll_switch' -Wl,-e,'$dll_entry' -o $output_objdir/$soname '$ltdll_obj'$libobjs $deplibs $compiler_flags'
    ;;

  netbsd*)
    if echo __ELF__ | $CC -E - | grep __ELF__ >/dev/null; then
      archive_cmds='$LD -Bshareable $libobjs $deplibs $linker_flags -o $lib'
      wlarc=
    else
      archive_cmds='$CC -shared -nodefaultlibs $libobjs $deplibs $compiler_flags ${wl}-soname $wl$soname -o $lib'
      archive_expsym_cmds='$CC -shared -nodefaultlibs $libobjs $deplibs $compiler_flags ${wl}-soname $wl$soname ${wl}-retain-symbols-file $wl$export_symbols -o $lib'
    fi
    ;;

  solaris* | sysv5*)
    if $LD -v 2>&1 | egrep 'BFD 2\.8' > /dev/null; then
      ld_shlibs=no
      cat <<EOF 1>&2

*** Warning: The releases 2.8.* of the GNU linker cannot reliably
*** create shared libraries on Solaris systems.  Therefore, libtool
*** is disabling shared libraries support.  We urge you to upgrade GNU
*** binutils to release 2.9.1 or newer.  Another option is to modify
*** your PATH or compiler configuration so that the native linker is
*** used, and then restart.

EOF
    elif $LD --help 2>&1 | egrep ': supported targets:.* elf' > /dev/null; then
      archive_cmds='$CC -shared $libobjs $deplibs $compiler_flags ${wl}-soname $wl$soname -o $lib'
      archive_expsym_cmds='$CC -shared $libobjs $deplibs $compiler_flags ${wl}-soname $wl$soname ${wl}-retain-symbols-file $wl$export_symbols -o $lib'
    else
      ld_shlibs=no
    fi
    ;;

  sunos4*)
    archive_cmds='$LD -assert pure-text -Bshareable -o $lib $libobjs $deplibs $linker_flags'
    wlarc=
    hardcode_direct=yes
    hardcode_shlibpath_var=no
    ;;

  *)
    if $LD --help 2>&1 | egrep ': supported targets:.* elf' > /dev/null; then
      archive_cmds='$CC -shared $libobjs $deplibs $compiler_flags ${wl}-soname $wl$soname -o $lib'
      archive_expsym_cmds='$CC -shared $libobjs $deplibs $compiler_flags ${wl}-soname $wl$soname ${wl}-retain-symbols-file $wl$export_symbols -o $lib'
    else
      ld_shlibs=no
    fi
    ;;
  esac

  if test "$ld_shlibs" = yes; then
    runpath_var=LD_RUN_PATH
    hardcode_libdir_flag_spec='${wl}--rpath ${wl}$libdir'
    export_dynamic_flag_spec='${wl}--export-dynamic'
    case $host_os in
    cygwin* | mingw* | pw32*)
      # dlltool doesn't understand --whole-archive et. al.
      whole_archive_flag_spec=
      ;;
    *)
      # ancient GNU ld didn't support --whole-archive et. al.
      if $LD --help 2>&1 | egrep 'no-whole-archive' > /dev/null; then
	whole_archive_flag_spec="$wlarc"'--whole-archive$convenience '"$wlarc"'--no-whole-archive'
      else
	whole_archive_flag_spec=
      fi
      ;;
    esac
  fi
else
  # PORTME fill in a description of your system's linker (not GNU ld)
  case $host_os in
  aix3*)
    allow_undefined_flag=unsupported
    always_export_symbols=yes
    archive_expsym_cmds='$LD -o $output_objdir/$soname $libobjs $deplibs $linker_flags -bE:$export_symbols -T512 -H512 -bM:SRE~$AR $AR_FLAGS $lib $output_objdir/$soname'
    # Note: this linker hardcodes the directories in LIBPATH if there
    # are no directories specified by -L.
    hardcode_minus_L=yes
    if test "$GCC" = yes && test -z "$link_static_flag"; then
      # Neither direct hardcoding nor static linking is supported with a
      # broken collect2.
      hardcode_direct=unsupported
    fi
    ;;

  aix4* | aix5*)
    # When large executables or shared objects are built, AIX ld can
    # have problems creating the table of contents.  If linking a library
    # or program results in "error TOC overflow" add -mminimal-toc to
    # CXXFLAGS/CFLAGS for g++/gcc.  In the cases where that is not
    # enough to fix the problem, add -Wl,-bbigtoc to LDFLAGS.

    archive_cmds=''
    hardcode_libdir_separator=':'
    if test "$GCC" = yes; then
      collect2name=`${CC} -print-prog-name=collect2`
      if test -f "$collect2name" && \
	 strings "$collect2name" | grep resolve_lib_name >/dev/null
      then
	# We have reworked collect2
	hardcode_direct=yes
      else
        # We have old collect2
        hardcode_direct=unsupported
        # It fails to find uninstalled libraries when the uninstalled
        # path is not listed in the libpath.  Setting hardcode_minus_L
        # to unsupported forces relinking
        hardcode_minus_L=yes
        hardcode_libdir_flag_spec='-L$libdir'
        hardcode_libdir_separator=
      fi
      shared_flag='-shared'
    else
      if test "$host_cpu" = ia64; then
        shared_flag='-G'
      else
        shared_flag='${wl}-bM:SRE'
      fi
      hardcode_direct=yes
    fi

    if test "$host_cpu" = ia64; then
      # On IA64, the linker does run time linking by default, so we don't
      # have to do anything special.
      aix_use_runtimelinking=no
      exp_sym_flag='-Bexport'
      no_entry_flag=""
    else
      # Test if we are trying to use run time linking, or normal AIX style linking.
      # If -brtl is somewhere in LDFLAGS, we need to do run time linking.
      aix_use_runtimelinking=no
      for ld_flag in $LDFLAGS; do
        if (test $ld_flag = "-brtl" || test $ld_flag = "-Wl,-brtl" ); then
          aix_use_runtimelinking=yes
          break
        fi
      done
      exp_sym_flag='-bexport'
      no_entry_flag='-bnoentry'
    fi
    # It seems that -bexpall can do strange things, so it is better to
    # generate a list of symbols to export.
    always_export_symbols=yes
    if test "$aix_use_runtimelinking" = yes; then
      hardcode_libdir_flag_spec='${wl}-blibpath:$libdir:/usr/lib:/lib'
      allow_undefined_flag=' -Wl,-G'
      archive_expsym_cmds="\$CC $shared_flag"' -o $output_objdir/$soname $libobjs $deplibs $compiler_flags ${allow_undefined_flag} '"\${wl}$no_entry_flag \${wl}$exp_sym_flag:\$export_symbols"
    else
      if test "$host_cpu" = ia64; then
        hardcode_libdir_flag_spec='${wl}-R $libdir:/usr/lib:/lib'
       allow_undefined_flag="-znodefs"
        archive_expsym_cmds="\$CC $shared_flag"' -o $output_objdir/$soname ${wl}-h$soname $libobjs $deplibs $compiler_flags ${wl}${allow_undefined_flag} '"\${wl}$no_entry_flag \${wl}$exp_sym_flag:\$export_symbols"
      else
        hardcode_libdir_flag_spec='${wl}-bnolibpath ${wl}-blibpath:$libdir:/usr/lib:/lib'
        # Warning - without using the other run time loading flags, -berok will
        #           link without error, but may produce a broken library.
        allow_undefined_flag='${wl}-berok"
        # This is a bit strange, but is similar to how AIX traditionally builds
        # it's shared libraries.
        archive_expsym_cmds="\$CC $shared_flag"' -o $output_objdir/$soname $libobjs $deplibs $compiler_flags ${allow_undefined_flag} '"\${wl}$no_entry_flag \${wl}$exp_sym_flag:\$export_symbols"' ~$AR -crlo $objdir/$libname$release.a $objdir/$soname'
      fi
    fi
    ;;

  amigaos*)
    archive_cmds='$rm $output_objdir/a2ixlibrary.data~$echo "#define NAME $libname" > $output_objdir/a2ixlibrary.data~$echo "#define LIBRARY_ID 1" >> $output_objdir/a2ixlibrary.data~$echo "#define VERSION $major" >> $output_objdir/a2ixlibrary.data~$echo "#define REVISION $revision" >> $output_objdir/a2ixlibrary.data~$AR $AR_FLAGS $lib $libobjs~$RANLIB $lib~(cd $output_objdir && a2ixlibrary -32)'
    hardcode_libdir_flag_spec='-L$libdir'
    hardcode_minus_L=yes
    # see comment about different semantics on the GNU ld section
    ld_shlibs=no
    ;;

  cygwin* | mingw* | pw32*)
    # When not using gcc, we currently assume that we are using
    # Microsoft Visual C++.
    # hardcode_libdir_flag_spec is actually meaningless, as there is
    # no search path for DLLs.
    hardcode_libdir_flag_spec=' '
    allow_undefined_flag=unsupported
    # Tell ltmain to make .lib files, not .a files.
    libext=lib
    # FIXME: Setting linknames here is a bad hack.
    archive_cmds='$CC -o $lib $libobjs $compiler_flags `echo "$deplibs" | sed -e '\''s/ -lc$//'\''` -link -dll~linknames='
    # The linker will automatically build a .lib file if we build a DLL.
    old_archive_from_new_cmds='true'
    # FIXME: Should let the user specify the lib program.
    old_archive_cmds='lib /OUT:$oldlib$oldobjs$old_deplibs'
    fix_srcfile_path='`cygpath -w "$srcfile"`'
    ;;

  darwin* | rhapsody*)
    allow_undefined_flag='-undefined suppress'
    # FIXME: Relying on posixy $() will cause problems for
    #        cross-compilation, but unfortunately the echo tests do not
    #        yet detect zsh echo's removal of \ escapes.
    archive_cmds='$CC $(test .$module = .yes && echo -bundle || echo -dynamiclib) $allow_undefined_flag -o $lib $libobjs $deplibs$linkopts -install_name $rpath/$soname $(test -n "$verstring" -a x$verstring != x0.0 && echo $verstring)'
    # We need to add '_' to the symbols in $export_symbols first
    #archive_expsym_cmds="$archive_cmds"' && strip -s $export_symbols'
    hardcode_direct=yes
    hardcode_shlibpath_var=no
    whole_archive_flag_spec='-all_load $convenience'
    ;;

  freebsd1*)
    ld_shlibs=no
    ;;

  # FreeBSD 2.2.[012] allows us to include c++rt0.o to get C++ constructor
  # support.  Future versions do this automatically, but an explicit c++rt0.o
  # does not break anything, and helps significantly (at the cost of a little
  # extra space).
  freebsd2.2*)
    archive_cmds='$LD -Bshareable -o $lib $libobjs $deplibs $linker_flags /usr/lib/c++rt0.o'
    hardcode_libdir_flag_spec='-R$libdir'
    hardcode_direct=yes
    hardcode_shlibpath_var=no
    ;;

  # Unfortunately, older versions of FreeBSD 2 do not have this feature.
  freebsd2*)
    archive_cmds='$LD -Bshareable -o $lib $libobjs $deplibs $linker_flags'
    hardcode_direct=yes
    hardcode_minus_L=yes
    hardcode_shlibpath_var=no
    ;;

  # FreeBSD 3 and greater uses gcc -shared to do shared libraries.
  freebsd*)
    archive_cmds='$CC -shared -o $lib $libobjs $deplibs $compiler_flags'
    hardcode_libdir_flag_spec='-R$libdir'
    hardcode_direct=yes
    hardcode_shlibpath_var=no
    ;;

  hpux9* | hpux10* | hpux11*)
    case $host_os in
    hpux9*) archive_cmds='$rm $output_objdir/$soname~$LD -b +b $install_libdir -o $output_objdir/$soname $libobjs $deplibs $linker_flags~test $output_objdir/$soname = $lib || mv $output_objdir/$soname $lib' ;;
    *) archive_cmds='$LD -b +h $soname +b $install_libdir -o $lib $libobjs $deplibs $linker_flags' ;;
    esac
    hardcode_libdir_flag_spec='${wl}+b ${wl}$libdir'
    hardcode_libdir_separator=:
    hardcode_direct=yes
    hardcode_minus_L=yes # Not in the search PATH, but as the default
			 # location of the library.
    export_dynamic_flag_spec='${wl}-E'
    ;;

  irix5* | irix6*)
    if test "$GCC" = yes; then
      archive_cmds='$CC -shared $libobjs $deplibs $compiler_flags ${wl}-soname ${wl}$soname `test -n "$verstring" && echo ${wl}-set_version ${wl}$verstring` ${wl}-update_registry ${wl}${output_objdir}/so_locations -o $lib'
    else
      archive_cmds='$LD -shared $libobjs $deplibs $linker_flags -soname $soname `test -n "$verstring" && echo -set_version $verstring` -update_registry ${output_objdir}/so_locations -o $lib'
    fi
    hardcode_libdir_flag_spec='${wl}-rpath ${wl}$libdir'
    hardcode_libdir_separator=:
    link_all_deplibs=yes
    ;;

  netbsd*)
    if echo __ELF__ | $CC -E - | grep __ELF__ >/dev/null; then
      archive_cmds='$LD -Bshareable -o $lib $libobjs $deplibs $linker_flags'  # a.out
    else
      archive_cmds='$LD -shared -o $lib $libobjs $deplibs $linker_flags'      # ELF
    fi
    hardcode_libdir_flag_spec='-R$libdir'
    hardcode_direct=yes
    hardcode_shlibpath_var=no
    ;;

  newsos6)
    archive_cmds='$LD -G -h $soname -o $lib $libobjs $deplibs $linkopts'
    hardcode_direct=yes
    hardcode_libdir_flag_spec='${wl}-rpath ${wl}$libdir'
    hardcode_libdir_separator=:
    hardcode_shlibpath_var=no
    ;;

  openbsd*)
    archive_cmds='$LD -Bshareable -o $lib $libobjs $deplibs $linker_flags'
    hardcode_libdir_flag_spec='-R$libdir'
    hardcode_direct=yes
    hardcode_shlibpath_var=no
    ;;

  os2*)
    hardcode_libdir_flag_spec='-L$libdir'
    hardcode_minus_L=yes
    allow_undefined_flag=unsupported
    archive_cmds='$echo "LIBRARY $libname INITINSTANCE" > $output_objdir/$libname.def~$echo "DESCRIPTION \"$libname\"" >> $output_objdir/$libname.def~$echo DATA >> $output_objdir/$libname.def~$echo " SINGLE NONSHARED" >> $output_objdir/$libname.def~$echo EXPORTS >> $output_objdir/$libname.def~emxexp $libobjs >> $output_objdir/$libname.def~$CC -Zdll -Zcrtdll -o $lib $libobjs $deplibs $compiler_flags $output_objdir/$libname.def'
    old_archive_from_new_cmds='emximp -o $output_objdir/$libname.a $output_objdir/$libname.def'
    ;;

  osf3*)
    if test "$GCC" = yes; then
      allow_undefined_flag=' ${wl}-expect_unresolved ${wl}\*'
      archive_cmds='$CC -shared${allow_undefined_flag} $libobjs $deplibs $compiler_flags ${wl}-soname ${wl}$soname `test -n "$verstring" && echo ${wl}-set_version ${wl}$verstring` ${wl}-update_registry ${wl}${output_objdir}/so_locations -o $lib'
    else
      allow_undefined_flag=' -expect_unresolved \*'
      archive_cmds='$LD -shared${allow_undefined_flag} $libobjs $deplibs $linker_flags -soname $soname `test -n "$verstring" && echo -set_version $verstring` -update_registry ${output_objdir}/so_locations -o $lib'
    fi
    hardcode_libdir_flag_spec='${wl}-rpath ${wl}$libdir'
    hardcode_libdir_separator=:
    ;;

  osf4* | osf5*)	# as osf3* with the addition of -msym flag
    if test "$GCC" = yes; then
      allow_undefined_flag=' ${wl}-expect_unresolved ${wl}\*'
      archive_cmds='$CC -shared${allow_undefined_flag} $libobjs $deplibs $compiler_flags ${wl}-msym ${wl}-soname ${wl}$soname `test -n "$verstring" && echo ${wl}-set_version ${wl}$verstring` ${wl}-update_registry ${wl}${output_objdir}/so_locations -o $lib'
      hardcode_libdir_flag_spec='${wl}-rpath ${wl}$libdir'
    else
      allow_undefined_flag=' -expect_unresolved \*'
      archive_cmds='$LD -shared${allow_undefined_flag} $libobjs $deplibs $linker_flags -msym -soname $soname `test -n "$verstring" && echo -set_version $verstring` -update_registry ${output_objdir}/so_locations -o $lib'
      archive_expsym_cmds='for i in `cat $export_symbols`; do printf "-exported_symbol " >> $lib.exp; echo "\$i" >> $lib.exp; done; echo "-hidden">> $lib.exp~
      $LD -shared${allow_undefined_flag} -input $lib.exp $linker_flags $libobjs $deplibs -soname $soname `test -n "$verstring" && echo -set_version $verstring` -update_registry ${objdir}/so_locations -o $lib~$rm $lib.exp'

      #Both c and cxx compiler support -rpath directly
      hardcode_libdir_flag_spec='-rpath $libdir'
    fi
    hardcode_libdir_separator=:
    ;;

  sco3.2v5*)
    archive_cmds='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
    hardcode_shlibpath_var=no
    runpath_var=LD_RUN_PATH
    hardcode_runpath_var=yes
    ;;

  solaris*)
    no_undefined_flag=' -z defs'
    # $CC -shared without GNU ld will not create a library from C++
    # object files and a static libstdc++, better avoid it by now
    archive_cmds='$LD -G${allow_undefined_flag} -h $soname -o $lib $libobjs $deplibs $linker_flags'
    archive_expsym_cmds='$echo "{ global:" > $lib.exp~cat $export_symbols | sed -e "s/\(.*\)/\1;/" >> $lib.exp~$echo "local: *; };" >> $lib.exp~
		$LD -G${allow_undefined_flag} -M $lib.exp -h $soname -o $lib $libobjs $deplibs $linker_flags~$rm $lib.exp'
    hardcode_libdir_flag_spec='-R$libdir'
    hardcode_shlibpath_var=no
    case $host_os in
    [solaris2.[0-5] | solaris2.[0-5].*]) ;;
    *) # Supported since Solaris 2.6 (maybe 2.5.1?)
      whole_archive_flag_spec='-z allextract$convenience -z defaultextract' ;;
    esac
    link_all_deplibs=yes
    ;;

  sunos4*)
    if test "x$host_vendor" = xsequent; then
      # Use $CC to link under sequent, because it throws in some extra .o
      # files that make .init and .fini sections work.
      archive_cmds='$CC -G ${wl}-h $soname -o $lib $libobjs $deplibs $compiler_flags'
    else
      archive_cmds='$LD -assert pure-text -Bstatic -o $lib $libobjs $deplibs $linker_flags'
    fi
    hardcode_libdir_flag_spec='-L$libdir'
    hardcode_direct=yes
    hardcode_minus_L=yes
    hardcode_shlibpath_var=no
    ;;

  sysv4)
    if test "x$host_vendor" = xsno; then
      archive_cmds='$LD -G -Bsymbolic -h $soname -o $lib $libobjs $deplibs $linkopts'
      hardcode_direct=yes # is this really true???
    else
      archive_cmds='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
      hardcode_direct=no #Motorola manual says yes, but my tests say they lie
    fi
    runpath_var='LD_RUN_PATH'
    hardcode_shlibpath_var=no
    ;;

  sysv4.3*)
    archive_cmds='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
    hardcode_shlibpath_var=no
    export_dynamic_flag_spec='-Bexport'
    ;;

  sysv5*)
    no_undefined_flag=' -z text'
    # $CC -shared without GNU ld will not create a library from C++
    # object files and a static libstdc++, better avoid it by now
    archive_cmds='$LD -G${allow_undefined_flag} -h $soname -o $lib $libobjs $deplibs $linker_flags'
    archive_expsym_cmds='$echo "{ global:" > $lib.exp~cat $export_symbols | sed -e "s/\(.*\)/\1;/" >> $lib.exp~$echo "local: *; };" >> $lib.exp~
		$LD -G${allow_undefined_flag} -M $lib.exp -h $soname -o $lib $libobjs $deplibs $linker_flags~$rm $lib.exp'
    hardcode_libdir_flag_spec=
    hardcode_shlibpath_var=no
    runpath_var='LD_RUN_PATH'
    ;;

  uts4*)
    archive_cmds='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
    hardcode_libdir_flag_spec='-L$libdir'
    hardcode_shlibpath_var=no
    ;;

  dgux*)
    archive_cmds='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
    hardcode_libdir_flag_spec='-L$libdir'
    hardcode_shlibpath_var=no
    ;;

  sysv4*MP*)
    if test -d /usr/nec; then
      archive_cmds='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
      hardcode_shlibpath_var=no
      runpath_var=LD_RUN_PATH
      hardcode_runpath_var=yes
      ld_shlibs=yes
    fi
    ;;

  sysv4.2uw2*)
    archive_cmds='$LD -G -o $lib $libobjs $deplibs $linker_flags'
    hardcode_direct=yes
    hardcode_minus_L=no
    hardcode_shlibpath_var=no
    hardcode_runpath_var=yes
    runpath_var=LD_RUN_PATH
    ;;

  sysv5uw7* | unixware7*)
    no_undefined_flag='${wl}-z ${wl}text'
    if test "$GCC" = yes; then
      archive_cmds='$CC -shared ${wl}-h ${wl}$soname -o $lib $libobjs $deplibs $compiler_flags'
    else
      archive_cmds='$CC -G ${wl}-h ${wl}$soname -o $lib $libobjs $deplibs $compiler_flags'
    fi
    runpath_var='LD_RUN_PATH'
    hardcode_shlibpath_var=no
    ;;

  *)
    ld_shlibs=no
    ;;
  esac
fi
AC_MSG_RESULT([$ld_shlibs])
test "$ld_shlibs" = no && can_build_shared=no

# Check hardcoding attributes.
AC_MSG_CHECKING([how to hardcode library paths into programs])
hardcode_action=
if test -n "$hardcode_libdir_flag_spec" || \
   test -n "$runpath_var"; then

  # We can hardcode non-existant directories.
  if test "$hardcode_direct" != no &&
     # If the only mechanism to avoid hardcoding is shlibpath_var, we
     # have to relink, otherwise we might link with an installed library
     # when we should be linking with a yet-to-be-installed one
     ## test "$hardcode_shlibpath_var" != no &&
     test "$hardcode_minus_L" != no; then
    # Linking always hardcodes the temporary library directory.
    hardcode_action=relink
  else
    # We can link without hardcoding, and we can hardcode nonexisting dirs.
    hardcode_action=immediate
  fi
else
  # We cannot hardcode anything, or else we can only hardcode existing
  # directories.
  hardcode_action=unsupported
fi
AC_MSG_RESULT([$hardcode_action])

striplib=
old_striplib=
AC_MSG_CHECKING([whether stripping libraries is possible])
if test -n "$STRIP" && $STRIP -V 2>&1 | grep "GNU strip" >/dev/null; then
  test -z "$old_striplib" && old_striplib="$STRIP --strip-debug"
  test -z "$striplib" && striplib="$STRIP --strip-unneeded"
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

reload_cmds='$LD$reload_flag -o $output$reload_objs'
test -z "$deplibs_check_method" && deplibs_check_method=unknown

# PORTME Fill in your ld.so characteristics
AC_MSG_CHECKING([dynamic linker characteristics])
library_names_spec=
libname_spec='lib$name'
soname_spec=
postinstall_cmds=
postuninstall_cmds=
finish_cmds=
finish_eval=
shlibpath_var=
shlibpath_overrides_runpath=unknown
version_type=none
dynamic_linker="$host_os ld.so"
sys_lib_dlsearch_path_spec="/lib /usr/lib"
sys_lib_search_path_spec="/lib /usr/lib /usr/local/lib"

case $host_os in
aix3*)
  version_type=linux
  library_names_spec='${libname}${release}.so$versuffix $libname.a'
  shlibpath_var=LIBPATH

  # AIX has no versioning support, so we append a major version to the name.
  soname_spec='${libname}${release}.so$major'
  ;;

aix4* | aix5*)
  version_type=linux
  if test "$host_cpu" = ia64; then
    # AIX 5 supports IA64
    library_names_spec='${libname}${release}.so$major ${libname}${release}.so$versuffix $libname.so'
    shlibpath_var=LD_LIBRARY_PATH
  else
    # With GCC up to 2.95.x, collect2 would create an import file
    # for dependence libraries.  The import file would start with
    # the line `#! .'.  This would cause the generated library to
    # depend on `.', always an invalid library.  This was fixed in
    # development snapshots of GCC prior to 3.0.
    case $host_os in
      [ aix4 | aix4.[01] | aix4.[01].*)]
      if { echo '#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 97)'
           echo ' yes '
           echo '#endif'; } | ${CC} -E - | grep yes > /dev/null; then
        :
      else
        can_build_shared=no
      fi
      ;;
    esac
    # AIX (on Power*) has no versioning support, so currently we can not hardcode correct
    # soname into executable. Probably we can add versioning support to
    # collect2, so additional links can be useful in future.
    if test "$aix_use_runtimelinking" = yes; then
      # If using run time linking (on AIX 4.2 or later) use lib<name>.so instead of
      # lib<name>.a to let people know that these are not typical AIX shared libraries.
      library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major $libname.so'
    else
      # We preserve .a as extension for shared libraries through AIX4.2
      # and later when we are not doing run time linking.
      library_names_spec='${libname}${release}.a $libname.a'
      soname_spec='${libname}${release}.so$major'
    fi
    shlibpath_var=LIBPATH
    deplibs_check_method=pass_all
  fi
  ;;

amigaos*)
  library_names_spec='$libname.ixlibrary $libname.a'
  # Create ${libname}_ixlibrary.a entries in /sys/libs.
  finish_eval='for lib in `ls $libdir/*.ixlibrary 2>/dev/null`; do libname=`$echo "X$lib" | [$Xsed -e '\''s%^.*/\([^/]*\)\.ixlibrary$%\1%'\'']`; test $rm /sys/libs/${libname}_ixlibrary.a; $show "(cd /sys/libs && $LN_S $lib ${libname}_ixlibrary.a)"; (cd /sys/libs && $LN_S $lib ${libname}_ixlibrary.a) || exit 1; done'
  ;;

beos*)
  library_names_spec='${libname}.so'
  dynamic_linker="$host_os ld.so"
  shlibpath_var=LIBRARY_PATH
  ;;

bsdi4*)
  version_type=linux
  need_version=no
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major $libname.so'
  soname_spec='${libname}${release}.so$major'
  finish_cmds='PATH="\$PATH:/sbin" ldconfig $libdir'
  shlibpath_var=LD_LIBRARY_PATH
  sys_lib_search_path_spec="/shlib /usr/lib /usr/X11/lib /usr/contrib/lib /lib /usr/local/lib"
  sys_lib_dlsearch_path_spec="/shlib /usr/lib /usr/local/lib"
  export_dynamic_flag_spec=-rdynamic
  # the default ld.so.conf also contains /usr/contrib/lib and
  # /usr/X11R6/lib (/usr/X11 is a link to /usr/X11R6), but let us allow
  # libtool to hard-code these into programs
  ;;

cygwin* | mingw* | pw32*)
  version_type=windows
  need_version=no
  need_lib_prefix=no
  case $GCC,$host_os in
  yes,cygwin*)
    library_names_spec='$libname.dll.a'
    soname_spec='`echo ${libname} | sed -e 's/^lib/cyg/'``echo ${release} | [sed -e 's/[.]/-/g']`${versuffix}.dll'
    postinstall_cmds='dlpath=`bash 2>&1 -c '\''. $dir/${file}i;echo \$dlname'\''`~
      dldir=$destdir/`dirname \$dlpath`~
      test -d \$dldir || mkdir -p \$dldir~
      $install_prog .libs/$dlname \$dldir/$dlname'
    postuninstall_cmds='dldll=`bash 2>&1 -c '\''. $file; echo \$dlname'\''`~
      dlpath=$dir/\$dldll~
       $rm \$dlpath'
    ;;
  yes,mingw*)
    library_names_spec='${libname}`echo ${release} | [sed -e 's/[.]/-/g']`${versuffix}.dll'
    sys_lib_search_path_spec=`$CC -print-search-dirs | grep "^libraries:" | sed -e "s/^libraries://" -e "s/;/ /g"`
    ;;
  yes,pw32*)
    library_names_spec='`echo ${libname} | sed -e 's/^lib/pw/'``echo ${release} | sed -e 's/[.]/-/g'`${versuffix}.dll'
    ;;
  *)
    library_names_spec='${libname}`echo ${release} | [sed -e 's/[.]/-/g']`${versuffix}.dll $libname.lib'
    ;;
  esac
  dynamic_linker='Win32 ld.exe'
  # FIXME: first we should search . and the directory the executable is in
  shlibpath_var=PATH
  ;;

darwin* | rhapsody*)
  dynamic_linker="$host_os dyld"
  version_type=darwin
  need_lib_prefix=no
  need_version=no
  # FIXME: Relying on posixy $() will cause problems for
  #        cross-compilation, but unfortunately the echo tests do not
  #        yet detect zsh echo's removal of \ escapes.
  library_names_spec='${libname}${release}${versuffix}.$(test .$module = .yes && echo so || echo dylib) ${libname}${release}${major}.$(test .$module = .yes && echo so || echo dylib) ${libname}.$(test .$module = .yes && echo so || echo dylib)'
  soname_spec='${libname}${release}${major}.$(test .$module = .yes && echo so || echo dylib)'
  shlibpath_overrides_runpath=yes
  shlibpath_var=DYLD_LIBRARY_PATH
  ;;

freebsd1*)
  dynamic_linker=no
  ;;

freebsd*)
  objformat=`test -x /usr/bin/objformat && /usr/bin/objformat || echo aout`
  version_type=freebsd-$objformat
  case $version_type in
    freebsd-elf*)
      library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so $libname.so'
      need_version=no
      need_lib_prefix=no
      ;;
    freebsd-*)
      library_names_spec='${libname}${release}.so$versuffix $libname.so$versuffix'
      need_version=yes
      ;;
  esac
  shlibpath_var=LD_LIBRARY_PATH
  case $host_os in
  freebsd2*)
    shlibpath_overrides_runpath=yes
    ;;
  *)
    shlibpath_overrides_runpath=no
    hardcode_into_libs=yes
    ;;
  esac
  ;;

gnu*)
  version_type=linux
  need_lib_prefix=no
  need_version=no
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so${major} ${libname}.so'
  soname_spec='${libname}${release}.so$major'
  shlibpath_var=LD_LIBRARY_PATH
  hardcode_into_libs=yes
  ;;

hpux9* | hpux10* | hpux11*)
  # Give a soname corresponding to the major version so that dld.sl refuses to
  # link against other versions.
  dynamic_linker="$host_os dld.sl"
  version_type=sunos
  need_lib_prefix=no
  need_version=no
  shlibpath_var=SHLIB_PATH
  shlibpath_overrides_runpath=no # +s is required to enable SHLIB_PATH
  library_names_spec='${libname}${release}.sl$versuffix ${libname}${release}.sl$major $libname.sl'
  soname_spec='${libname}${release}.sl$major'
  # HP-UX runs *really* slowly unless shared libraries are mode 555.
  postinstall_cmds='chmod 555 $lib'
  ;;

irix5* | irix6*)
  version_type=irix
  need_lib_prefix=no
  need_version=no
  soname_spec='${libname}${release}.so$major'
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major ${libname}${release}.so $libname.so'
  case $host_os in
  irix5*)
    libsuff= shlibsuff=
    ;;
  *)
    case $LD in # libtool.m4 will add one of these switches to LD
    *-32|*"-32 ") libsuff= shlibsuff= libmagic=32-bit;;
    *-n32|*"-n32 ") libsuff=32 shlibsuff=N32 libmagic=N32;;
    *-64|*"-64 ") libsuff=64 shlibsuff=64 libmagic=64-bit;;
    *) libsuff= shlibsuff= libmagic=never-match;;
    esac
    ;;
  esac
  shlibpath_var=LD_LIBRARY${shlibsuff}_PATH
  shlibpath_overrides_runpath=no
  sys_lib_search_path_spec="/usr/lib${libsuff} /lib${libsuff} /usr/local/lib${libsuff}"
  sys_lib_dlsearch_path_spec="/usr/lib${libsuff} /lib${libsuff}"
  ;;

# No shared lib support for Linux oldld, aout, or coff.
linux-gnuoldld* | linux-gnuaout* | linux-gnucoff*)
  dynamic_linker=no
  ;;

# This must be Linux ELF.
linux-gnu*)
  version_type=linux
  need_lib_prefix=no
  need_version=no
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major $libname.so'
  soname_spec='${libname}${release}.so$major'
  finish_cmds='PATH="\$PATH:/sbin" ldconfig -n $libdir'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=no
  # This implies no fast_install, which is unacceptable.
  # Some rework will be needed to allow for fast_install
  # before this can be enabled.
  hardcode_into_libs=yes

  # We used to test for /lib/ld.so.1 and disable shared libraries on
  # powerpc, because MkLinux only supported shared libraries with the
  # GNU dynamic linker.  Since this was broken with cross compilers,
  # most powerpc-linux boxes support dynamic linking these days and
  # people can always --disable-shared, the test was removed, and we
  # assume the GNU/Linux dynamic linker is in use.
  dynamic_linker='GNU/Linux ld.so'
  ;;

netbsd*)
  version_type=sunos
  need_lib_prefix=no
  need_version=no
  if echo __ELF__ | $CC -E - | grep __ELF__ >/dev/null; then
    library_names_spec='${libname}${release}.so$versuffix ${libname}.so$versuffix'
    finish_cmds='PATH="\$PATH:/sbin" ldconfig -m $libdir'
    dynamic_linker='NetBSD (a.out) ld.so'
  else
    library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major ${libname}${release}.so ${libname}.so'
    soname_spec='${libname}${release}.so$major'
    dynamic_linker='NetBSD ld.elf_so'
  fi
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  hardcode_into_libs=yes
  ;;

newsos6)
  version_type=linux
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major $libname.so'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  ;;

openbsd*)
  version_type=sunos
  if test "$with_gnu_ld" = yes; then
    need_lib_prefix=no
    need_version=no
  fi
  library_names_spec='${libname}${release}.so$versuffix ${libname}.so$versuffix'
  finish_cmds='PATH="\$PATH:/sbin" ldconfig -m $libdir'
  shlibpath_var=LD_LIBRARY_PATH
  ;;

os2*)
  libname_spec='$name'
  need_lib_prefix=no
  library_names_spec='$libname.dll $libname.a'
  dynamic_linker='OS/2 ld.exe'
  shlibpath_var=LIBPATH
  ;;

osf3* | osf4* | osf5*)
  version_type=osf
  need_version=no
  soname_spec='${libname}${release}.so'
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so $libname.so'
  shlibpath_var=LD_LIBRARY_PATH
  sys_lib_search_path_spec="/usr/shlib /usr/ccs/lib /usr/lib/cmplrs/cc /usr/lib /usr/local/lib /var/shlib"
  sys_lib_dlsearch_path_spec="$sys_lib_search_path_spec"
  ;;

sco3.2v5*)
  version_type=osf
  soname_spec='${libname}${release}.so$major'
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major $libname.so'
  shlibpath_var=LD_LIBRARY_PATH
  ;;

solaris*)
  version_type=linux
  need_lib_prefix=no
  need_version=no
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major $libname.so'
  soname_spec='${libname}${release}.so$major'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  hardcode_into_libs=yes
  # ldd complains unless libraries are executable
  postinstall_cmds='chmod +x $lib'
  ;;

sunos4*)
  version_type=sunos
  library_names_spec='${libname}${release}.so$versuffix ${libname}.so$versuffix'
  finish_cmds='PATH="\$PATH:/usr/etc" ldconfig $libdir'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  if test "$with_gnu_ld" = yes; then
    need_lib_prefix=no
  fi
  need_version=yes
  ;;

sysv4 | sysv4.2uw2* | sysv4.3* | sysv5*)
  version_type=linux
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major $libname.so'
  soname_spec='${libname}${release}.so$major'
  shlibpath_var=LD_LIBRARY_PATH
  case $host_vendor in
    sni)
      shlibpath_overrides_runpath=no
      ;;
    motorola)
      need_lib_prefix=no
      need_version=no
      shlibpath_overrides_runpath=no
      sys_lib_search_path_spec='/lib /usr/lib /usr/ccs/lib'
      ;;
  esac
  ;;

uts4*)
  version_type=linux
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major $libname.so'
  soname_spec='${libname}${release}.so$major'
  shlibpath_var=LD_LIBRARY_PATH
  ;;

dgux*)
  version_type=linux
  need_lib_prefix=no
  need_version=no
  library_names_spec='${libname}${release}.so$versuffix ${libname}${release}.so$major $libname.so'
  soname_spec='${libname}${release}.so$major'
  shlibpath_var=LD_LIBRARY_PATH
  ;;

sysv4*MP*)
  if test -d /usr/nec ;then
    version_type=linux
    library_names_spec='$libname.so.$versuffix $libname.so.$major $libname.so'
    soname_spec='$libname.so.$major'
    shlibpath_var=LD_LIBRARY_PATH
  fi
  ;;

*)
  dynamic_linker=no
  ;;
esac
AC_MSG_RESULT([$dynamic_linker])
test "$dynamic_linker" = no && can_build_shared=no

# Report the final consequences.
AC_MSG_CHECKING([if libtool supports shared libraries])
AC_MSG_RESULT([$can_build_shared])

if test "$hardcode_action" = relink; then
  # Fast installation is not supported
  enable_fast_install=no
elif test "$shlibpath_overrides_runpath" = yes ||
     test "$enable_shared" = no; then
  # Fast installation is not necessary
  enable_fast_install=needless
fi

variables_saved_for_relink="PATH $shlibpath_var $runpath_var"
if test "$GCC" = yes; then
  variables_saved_for_relink="$variables_saved_for_relink GCC_EXEC_PREFIX COMPILER_PATH LIBRARY_PATH"
fi

AC_LIBTOOL_DLOPEN_SELF

if test "$enable_shared" = yes && test "$GCC" = yes; then
  case $archive_cmds in
  *'~'*)
    # FIXME: we may have to deal with multi-command sequences.
    ;;
  '$CC '*)
    # Test whether the compiler implicitly links with -lc since on some
    # systems, -lgcc has to come before -lc. If gcc already passes -lc
    # to ld, don't add -lc before -lgcc.
    AC_MSG_CHECKING([whether -lc should be explicitly linked in])
    AC_CACHE_VAL([lt_cv_archive_cmds_need_lc],
    [$rm conftest*
    echo 'static int dummy;' > conftest.$ac_ext

    if AC_TRY_EVAL(ac_compile); then
      soname=conftest
      lib=conftest
      libobjs=conftest.$ac_objext
      deplibs=
      wl=$lt_cv_prog_cc_wl
      compiler_flags=-v
      linker_flags=-v
      verstring=
      output_objdir=.
      libname=conftest
      save_allow_undefined_flag=$allow_undefined_flag
      allow_undefined_flag=
      if AC_TRY_EVAL(archive_cmds 2\>\&1 \| grep \" -lc \" \>/dev/null 2\>\&1)
      then
	lt_cv_archive_cmds_need_lc=no
      else
	lt_cv_archive_cmds_need_lc=yes
      fi
      allow_undefined_flag=$save_allow_undefined_flag
    else
      cat conftest.err 1>&5
    fi])
    AC_MSG_RESULT([$lt_cv_archive_cmds_need_lc])
    ;;
  esac
fi
need_lc=${lt_cv_archive_cmds_need_lc-yes}

# The second clause should only fire when bootstrapping the
# libtool distribution, otherwise you forgot to ship ltmain.sh
# with your package, and you will get complaints that there are
# no rules to generate ltmain.sh.
if test -f "$ltmain"; then
  :
else
  # If there is no Makefile yet, we rely on a make rule to execute
  # `config.status --recheck' to rerun these tests and create the
  # libtool script then.
  test -f Makefile && make "$ltmain"
fi

if test -f "$ltmain"; then
  trap "$rm \"${ofile}T\"; exit 1" 1 2 15
  $rm -f "${ofile}T"

  echo creating $ofile

  # Now quote all the things that may contain metacharacters while being
  # careful not to overquote the AC_SUBSTed values.  We take copies of the
  # variables and quote the copies for generation of the libtool script.
  for var in echo old_CC old_CFLAGS \
    AR AR_FLAGS CC LD LN_S NM SHELL \
    reload_flag reload_cmds wl \
    pic_flag link_static_flag no_builtin_flag export_dynamic_flag_spec \
    thread_safe_flag_spec whole_archive_flag_spec libname_spec \
    library_names_spec soname_spec \
    RANLIB old_archive_cmds old_archive_from_new_cmds old_postinstall_cmds \
    old_postuninstall_cmds archive_cmds archive_expsym_cmds postinstall_cmds \
    postuninstall_cmds extract_expsyms_cmds old_archive_from_expsyms_cmds \
    old_striplib striplib file_magic_cmd export_symbols_cmds \
    deplibs_check_method allow_undefined_flag no_undefined_flag \
    finish_cmds finish_eval global_symbol_pipe global_symbol_to_cdecl \
    hardcode_libdir_flag_spec hardcode_libdir_separator  \
    sys_lib_search_path_spec sys_lib_dlsearch_path_spec \
    compiler_c_o compiler_o_lo need_locks exclude_expsyms include_expsyms; do

    case $var in
    reload_cmds | old_archive_cmds | old_archive_from_new_cmds | \
    old_postinstall_cmds | old_postuninstall_cmds | \
    export_symbols_cmds | archive_cmds | archive_expsym_cmds | \
    extract_expsyms_cmds | old_archive_from_expsyms_cmds | \
    postinstall_cmds | postuninstall_cmds | \
    finish_cmds | sys_lib_search_path_spec | sys_lib_dlsearch_path_spec)
      # Double-quote double-evaled strings.
      eval "lt_$var=\\\"\`\$echo \"X\$$var\" | \$Xsed -e \"\$double_quote_subst\" -e \"\$sed_quote_subst\" -e \"\$delay_variable_subst\"\`\\\""
      ;;
    *)
      eval "lt_$var=\\\"\`\$echo \"X\$$var\" | \$Xsed -e \"\$sed_quote_subst\"\`\\\""
      ;;
    esac
  done

  cat <<__EOF__ > "${ofile}T"
#! $SHELL

# `$echo "$ofile" | sed 's%^.*/%%'` - Provide generalized library-building support services.
# Generated automatically by $PROGRAM (GNU $PACKAGE $VERSION$TIMESTAMP)
# NOTE: Changes made to this file will be lost: look at ltmain.sh.
#
# Copyright (C) 1996-2000 Free Software Foundation, Inc.
# Originally by Gordon Matzigkeit <gord@gnu.ai.mit.edu>, 1996
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# As a special exception to the GNU General Public License, if you
# distribute this file as part of a program that contains a
# configuration script generated by Autoconf, you may include it under
# the same distribution terms that you use for the rest of that program.

# Sed that helps us avoid accidentally triggering echo(1) options like -n.
Xsed="sed -e s/^X//"

# The HP-UX ksh and POSIX shell print the target directory to stdout
# if CDPATH is set.
if test "X\${CDPATH+set}" = Xset; then CDPATH=:; export CDPATH; fi

# ### BEGIN LIBTOOL CONFIG

# Libtool was configured on host `(hostname || uname -n) 2>/dev/null | sed 1q`:

# Shell to use when invoking shell scripts.
SHELL=$lt_SHELL

# Whether or not to build shared libraries.
build_libtool_libs=$enable_shared

# Whether or not to add -lc for building shared libraries.
build_libtool_need_lc=$need_lc

# Whether or not to build static libraries.
build_old_libs=$enable_static

# Whether or not to optimize for fast installation.
fast_install=$enable_fast_install

# The host system.
host_alias=$host_alias
host=$host

# An echo program that does not interpret backslashes.
echo=$lt_echo

# The archiver.
AR=$lt_AR
AR_FLAGS=$lt_AR_FLAGS

# The default C compiler.
CC=$lt_CC

# Is the compiler the GNU C compiler?
with_gcc=$GCC

# The linker used to build libraries.
LD=$lt_LD

# Whether we need hard or soft links.
LN_S=$lt_LN_S

# A BSD-compatible nm program.
NM=$lt_NM

# A symbol stripping program
STRIP=$STRIP

# Used to examine libraries when file_magic_cmd begins "file"
MAGIC_CMD=$MAGIC_CMD

# Used on cygwin: DLL creation program.
DLLTOOL="$DLLTOOL"

# Used on cygwin: object dumper.
OBJDUMP="$OBJDUMP"

# Used on cygwin: assembler.
AS="$AS"

# The name of the directory that contains temporary libtool files.
objdir=$objdir

# How to create reloadable object files.
reload_flag=$lt_reload_flag
reload_cmds=$lt_reload_cmds

# How to pass a linker flag through the compiler.
wl=$lt_wl

# Object file suffix (normally "o").
objext="$ac_objext"

# Old archive suffix (normally "a").
libext="$libext"

# Executable file suffix (normally "").
exeext="$exeext"

# Additional compiler flags for building library objects.
pic_flag=$lt_pic_flag
pic_mode=$pic_mode

# Does compiler simultaneously support -c and -o options?
compiler_c_o=$lt_compiler_c_o

# Can we write directly to a .lo ?
compiler_o_lo=$lt_compiler_o_lo

# Must we lock files when doing compilation ?
need_locks=$lt_need_locks

# Do we need the lib prefix for modules?
need_lib_prefix=$need_lib_prefix

# Do we need a version for libraries?
need_version=$need_version

# Whether dlopen is supported.
dlopen_support=$enable_dlopen

# Whether dlopen of programs is supported.
dlopen_self=$enable_dlopen_self

# Whether dlopen of statically linked programs is supported.
dlopen_self_static=$enable_dlopen_self_static

# Compiler flag to prevent dynamic linking.
link_static_flag=$lt_link_static_flag

# Compiler flag to turn off builtin functions.
no_builtin_flag=$lt_no_builtin_flag

# Compiler flag to allow reflexive dlopens.
export_dynamic_flag_spec=$lt_export_dynamic_flag_spec

# Compiler flag to generate shared objects directly from archives.
whole_archive_flag_spec=$lt_whole_archive_flag_spec

# Compiler flag to generate thread-safe objects.
thread_safe_flag_spec=$lt_thread_safe_flag_spec

# Library versioning type.
version_type=$version_type

# Format of library name prefix.
libname_spec=$lt_libname_spec

# List of archive names.  First name is the real one, the rest are links.
# The last name is the one that the linker finds with -lNAME.
library_names_spec=$lt_library_names_spec

# The coded name of the library, if different from the real name.
soname_spec=$lt_soname_spec

# Commands used to build and install an old-style archive.
RANLIB=$lt_RANLIB
old_archive_cmds=$lt_old_archive_cmds
old_postinstall_cmds=$lt_old_postinstall_cmds
old_postuninstall_cmds=$lt_old_postuninstall_cmds

# Create an old-style archive from a shared archive.
old_archive_from_new_cmds=$lt_old_archive_from_new_cmds

# Create a temporary old-style archive to link instead of a shared archive.
old_archive_from_expsyms_cmds=$lt_old_archive_from_expsyms_cmds

# Commands used to build and install a shared archive.
archive_cmds=$lt_archive_cmds
archive_expsym_cmds=$lt_archive_expsym_cmds
postinstall_cmds=$lt_postinstall_cmds
postuninstall_cmds=$lt_postuninstall_cmds

# Commands to strip libraries.
old_striplib=$lt_old_striplib
striplib=$lt_striplib

# Method to check whether dependent libraries are shared objects.
deplibs_check_method=$lt_deplibs_check_method

# Command to use when deplibs_check_method == file_magic.
file_magic_cmd=$lt_file_magic_cmd

# Flag that allows shared libraries with undefined symbols to be built.
allow_undefined_flag=$lt_allow_undefined_flag

# Flag that forces no undefined symbols.
no_undefined_flag=$lt_no_undefined_flag

# Commands used to finish a libtool library installation in a directory.
finish_cmds=$lt_finish_cmds

# Same as above, but a single script fragment to be evaled but not shown.
finish_eval=$lt_finish_eval

# Take the output of nm and produce a listing of raw symbols and C names.
global_symbol_pipe=$lt_global_symbol_pipe

# Transform the output of nm in a proper C declaration
global_symbol_to_cdecl=$lt_global_symbol_to_cdecl

# This is the shared library runtime path variable.
runpath_var=$runpath_var

# This is the shared library path variable.
shlibpath_var=$shlibpath_var

# Is shlibpath searched before the hard-coded library search path?
shlibpath_overrides_runpath=$shlibpath_overrides_runpath

# How to hardcode a shared library path into an executable.
hardcode_action=$hardcode_action

# Whether we should hardcode library paths into libraries.
hardcode_into_libs=$hardcode_into_libs

# Flag to hardcode \$libdir into a binary during linking.
# This must work even if \$libdir does not exist.
hardcode_libdir_flag_spec=$lt_hardcode_libdir_flag_spec

# Whether we need a single -rpath flag with a separated argument.
hardcode_libdir_separator=$lt_hardcode_libdir_separator

# Set to yes if using DIR/libNAME.so during linking hardcodes DIR into the
# resulting binary.
hardcode_direct=$hardcode_direct

# Set to yes if using the -LDIR flag during linking hardcodes DIR into the
# resulting binary.
hardcode_minus_L=$hardcode_minus_L

# Set to yes if using SHLIBPATH_VAR=DIR during linking hardcodes DIR into
# the resulting binary.
hardcode_shlibpath_var=$hardcode_shlibpath_var

# Variables whose values should be saved in libtool wrapper scripts and
# restored at relink time.
variables_saved_for_relink="$variables_saved_for_relink"

# Whether libtool must link a program against all its dependency libraries.
link_all_deplibs=$link_all_deplibs

# Compile-time system search path for libraries
sys_lib_search_path_spec=$lt_sys_lib_search_path_spec

# Run-time system search path for libraries
sys_lib_dlsearch_path_spec=$lt_sys_lib_dlsearch_path_spec

# Fix the shell variable \$srcfile for the compiler.
fix_srcfile_path="$fix_srcfile_path"

# Set to yes if exported symbols are required.
always_export_symbols=$always_export_symbols

# The commands to list exported symbols.
export_symbols_cmds=$lt_export_symbols_cmds

# The commands to extract the exported symbol list from a shared archive.
extract_expsyms_cmds=$lt_extract_expsyms_cmds

# Symbols that should not be listed in the preloaded symbols.
exclude_expsyms=$lt_exclude_expsyms

# Symbols that must always be exported.
include_expsyms=$lt_include_expsyms

# ### END LIBTOOL CONFIG

__EOF__

  case $host_os in
  aix3*)
    cat <<\EOF >> "${ofile}T"

# AIX sometimes has problems with the GCC collect2 program.  For some
# reason, if we set the COLLECT_NAMES environment variable, the problems
# vanish in a puff of smoke.
if test "X${COLLECT_NAMES+set}" != Xset; then
  COLLECT_NAMES=
  export COLLECT_NAMES
fi
EOF
    ;;
  esac

  case $host_os in
  cygwin* | mingw* | pw32* | os2*)
    cat <<'EOF' >> "${ofile}T"
      # This is a source program that is used to create dlls on Windows
      # Don't remove nor modify the starting and closing comments
# /* ltdll.c starts here */
# #define WIN32_LEAN_AND_MEAN
# #include <windows.h>
# #undef WIN32_LEAN_AND_MEAN
# #include <stdio.h>
#
# #ifndef __CYGWIN__
# #  ifdef __CYGWIN32__
# #    define __CYGWIN__ __CYGWIN32__
# #  endif
# #endif
#
# #ifdef __cplusplus
# extern "C" {
# #endif
# BOOL APIENTRY DllMain (HINSTANCE hInst, DWORD reason, LPVOID reserved);
# #ifdef __cplusplus
# }
# #endif
#
# #ifdef __CYGWIN__
# #include <cygwin/cygwin_dll.h>
# DECLARE_CYGWIN_DLL( DllMain );
# #endif
# HINSTANCE __hDllInstance_base;
#
# BOOL APIENTRY
# DllMain (HINSTANCE hInst, DWORD reason, LPVOID reserved)
# {
#   __hDllInstance_base = hInst;
#   return TRUE;
# }
# /* ltdll.c ends here */
        # This is a source program that is used to create import libraries
        # on Windows for dlls which lack them. Don't remove nor modify the
        # starting and closing comments
# /* impgen.c starts here */
# /*   Copyright (C) 1999-2000 Free Software Foundation, Inc.
#
#  This file is part of GNU libtool.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#  */
#
# #include <stdio.h>		/* for printf() */
# #include <unistd.h>		/* for open(), lseek(), read() */
# #include <fcntl.h>		/* for O_RDONLY, O_BINARY */
# #include <string.h>		/* for strdup() */
#
# /* O_BINARY isn't required (or even defined sometimes) under Unix */
# #ifndef O_BINARY
# #define O_BINARY 0
# #endif
#
# static unsigned int
# pe_get16 (fd, offset)
#      int fd;
#      int offset;
# {
#   unsigned char b[2];
#   lseek (fd, offset, SEEK_SET);
#   read (fd, b, 2);
#   return b[0] + (b[1]<<8);
# }
#
# static unsigned int
# pe_get32 (fd, offset)
#     int fd;
#     int offset;
# {
#   unsigned char b[4];
#   lseek (fd, offset, SEEK_SET);
#   read (fd, b, 4);
#   return b[0] + (b[1]<<8) + (b[2]<<16) + (b[3]<<24);
# }
#
# static unsigned int
# pe_as32 (ptr)
#      void *ptr;
# {
#   unsigned char *b = ptr;
#   return b[0] + (b[1]<<8) + (b[2]<<16) + (b[3]<<24);
# }
#
# int
# main (argc, argv)
#     int argc;
#     char *argv[];
# {
#     int dll;
#     unsigned long pe_header_offset, opthdr_ofs, num_entries, i;
#     unsigned long export_rva, export_size, nsections, secptr, expptr;
#     unsigned long name_rvas, nexp;
#     unsigned char *expdata, *erva;
#     char *filename, *dll_name;
#
#     filename = argv[1];
#
#     dll = open(filename, O_RDONLY|O_BINARY);
#     if (dll < 1)
# 	return 1;
#
#     dll_name = filename;
#
#     for (i=0; filename[i]; i++)
# 	if (filename[i] == '/' || filename[i] == '\\'  || filename[i] == ':')
# 	    dll_name = filename + i +1;
#
#     pe_header_offset = pe_get32 (dll, 0x3c);
#     opthdr_ofs = pe_header_offset + 4 + 20;
#     num_entries = pe_get32 (dll, opthdr_ofs + 92);
#
#     if (num_entries < 1) /* no exports */
# 	return 1;
#
#     export_rva = pe_get32 (dll, opthdr_ofs + 96);
#     export_size = pe_get32 (dll, opthdr_ofs + 100);
#     nsections = pe_get16 (dll, pe_header_offset + 4 +2);
#     secptr = (pe_header_offset + 4 + 20 +
# 	      pe_get16 (dll, pe_header_offset + 4 + 16));
#
#     expptr = 0;
#     for (i = 0; i < nsections; i++)
#     {
# 	char sname[8];
# 	unsigned long secptr1 = secptr + 40 * i;
# 	unsigned long vaddr = pe_get32 (dll, secptr1 + 12);
# 	unsigned long vsize = pe_get32 (dll, secptr1 + 16);
# 	unsigned long fptr = pe_get32 (dll, secptr1 + 20);
# 	lseek(dll, secptr1, SEEK_SET);
# 	read(dll, sname, 8);
# 	if (vaddr <= export_rva && vaddr+vsize > export_rva)
# 	{
# 	    expptr = fptr + (export_rva - vaddr);
# 	    if (export_rva + export_size > vaddr + vsize)
# 		export_size = vsize - (export_rva - vaddr);
# 	    break;
# 	}
#     }
#
#     expdata = (unsigned char*)malloc(export_size);
#     lseek (dll, expptr, SEEK_SET);
#     read (dll, expdata, export_size);
#     erva = expdata - export_rva;
#
#     nexp = pe_as32 (expdata+24);
#     name_rvas = pe_as32 (expdata+32);
#
#     printf ("EXPORTS\n");
#     for (i = 0; i<nexp; i++)
#     {
# 	unsigned long name_rva = pe_as32 (erva+name_rvas+i*4);
# 	printf ("\t%s @ %ld ;\n", erva+name_rva, 1+ i);
#     }
#
#     return 0;
# }
# /* impgen.c ends here */

EOF
    ;;
  esac

  # We use sed instead of cat because bash on DJGPP gets confused if
  # if finds mixed CR/LF and LF-only lines.  Since sed operates in
  # text mode, it properly converts lines to CR/LF.  This bash problem
  # is reportedly fixed, but why not run on old versions too?
  sed '$q' "$ltmain" >> "${ofile}T" || (rm -f "${ofile}T"; exit 1)

  mv -f "${ofile}T" "$ofile" || \
    (rm -f "$ofile" && cp "${ofile}T" "$ofile" && rm -f "${ofile}T")
  chmod +x "$ofile"
fi

])# _LT_AC_LTCONFIG_HACK

# AC_LIBTOOL_DLOPEN - enable checks for dlopen support
AC_DEFUN([AC_LIBTOOL_DLOPEN], [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])])

# AC_LIBTOOL_WIN32_DLL - declare package support for building win32 dll's
AC_DEFUN([AC_LIBTOOL_WIN32_DLL], [AC_BEFORE([$0], [AC_LIBTOOL_SETUP])])

# AC_ENABLE_SHARED - implement the --enable-shared flag
# Usage: AC_ENABLE_SHARED[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN([AC_ENABLE_SHARED],
[define([AC_ENABLE_SHARED_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(shared,
changequote(<<, >>)dnl
<<  --enable-shared[=PKGS]  build shared libraries [default=>>AC_ENABLE_SHARED_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case $enableval in
yes) enable_shared=yes ;;
no) enable_shared=no ;;
*)
  enable_shared=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_shared=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_shared=AC_ENABLE_SHARED_DEFAULT)dnl
])

# AC_DISABLE_SHARED - set the default shared flag to --disable-shared
AC_DEFUN([AC_DISABLE_SHARED],
[AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_SHARED(no)])

# AC_ENABLE_STATIC - implement the --enable-static flag
# Usage: AC_ENABLE_STATIC[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN([AC_ENABLE_STATIC],
[define([AC_ENABLE_STATIC_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(static,
changequote(<<, >>)dnl
<<  --enable-static[=PKGS]  build static libraries [default=>>AC_ENABLE_STATIC_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case $enableval in
yes) enable_static=yes ;;
no) enable_static=no ;;
*)
  enable_static=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_static=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_static=AC_ENABLE_STATIC_DEFAULT)dnl
])

# AC_DISABLE_STATIC - set the default static flag to --disable-static
AC_DEFUN([AC_DISABLE_STATIC],
[AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_STATIC(no)])


# AC_ENABLE_FAST_INSTALL - implement the --enable-fast-install flag
# Usage: AC_ENABLE_FAST_INSTALL[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN([AC_ENABLE_FAST_INSTALL],
[define([AC_ENABLE_FAST_INSTALL_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(fast-install,
changequote(<<, >>)dnl
<<  --enable-fast-install[=PKGS]  optimize for fast installation [default=>>AC_ENABLE_FAST_INSTALL_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case $enableval in
yes) enable_fast_install=yes ;;
no) enable_fast_install=no ;;
*)
  enable_fast_install=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_fast_install=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_fast_install=AC_ENABLE_FAST_INSTALL_DEFAULT)dnl
])

# AC_DISABLE_FAST_INSTALL - set the default to --disable-fast-install
AC_DEFUN([AC_DISABLE_FAST_INSTALL],
[AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_FAST_INSTALL(no)])

# AC_LIBTOOL_PICMODE - implement the --with-pic flag
# Usage: AC_LIBTOOL_PICMODE[(MODE)]
#   Where MODE is either `yes' or `no'.  If omitted, it defaults to
#   `both'.
AC_DEFUN([AC_LIBTOOL_PICMODE],
[AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
pic_mode=ifelse($#,1,$1,default)])


# AC_PATH_TOOL_PREFIX - find a file program which can recognise shared library
AC_DEFUN([AC_PATH_TOOL_PREFIX],
[AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(lt_cv_path_MAGIC_CMD,
[case $MAGIC_CMD in
  /*)
  lt_cv_path_MAGIC_CMD="$MAGIC_CMD" # Let the user override the test with a path.
  ;;
  ?:/*)
  lt_cv_path_MAGIC_CMD="$MAGIC_CMD" # Let the user override the test with a dos path.
  ;;
  *)
  ac_save_MAGIC_CMD="$MAGIC_CMD"
  IFS="${IFS=   }"; ac_save_ifs="$IFS"; IFS=":"
dnl $ac_dummy forces splitting on constant user-supplied paths.
dnl POSIX.2 word splitting is done only on the output of word expansions,
dnl not every word.  This closes a longstanding sh security hole.
  ac_dummy="ifelse([$2], , $PATH, [$2])"
  for ac_dir in $ac_dummy; do
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/$1; then
      lt_cv_path_MAGIC_CMD="$ac_dir/$1"
      if test -n "$file_magic_test_file"; then
	case $deplibs_check_method in
	"file_magic "*)
	  file_magic_regex="`expr \"$deplibs_check_method\" : \"file_magic \(.*\)\"`"
	  MAGIC_CMD="$lt_cv_path_MAGIC_CMD"
	  if eval $file_magic_cmd \$file_magic_test_file 2> /dev/null |
	    egrep "$file_magic_regex" > /dev/null; then
	    :
	  else
	    cat <<EOF 1>&2

*** Warning: the command libtool uses to detect shared libraries,
*** $file_magic_cmd, produces output that libtool cannot recognize.
*** The result is that libtool may fail to recognize shared libraries
*** as such.  This will affect the creation of libtool libraries that
*** depend on shared libraries, but programs linked with such libtool
*** libraries will work regardless of this problem.  Nevertheless, you
*** may want to report the problem to your system manager and/or to
*** bug-libtool@gnu.org

EOF
	  fi ;;
	esac
      fi
      break
    fi
  done
  IFS="$ac_save_ifs"
  MAGIC_CMD="$ac_save_MAGIC_CMD"
  ;;
esac])
MAGIC_CMD="$lt_cv_path_MAGIC_CMD"
if test -n "$MAGIC_CMD"; then
  AC_MSG_RESULT($MAGIC_CMD)
else
  AC_MSG_RESULT(no)
fi
])


# AC_PATH_MAGIC - find a file program which can recognise a shared library
AC_DEFUN([AC_PATH_MAGIC],
[AC_REQUIRE([AC_CHECK_TOOL_PREFIX])dnl
AC_PATH_TOOL_PREFIX(${ac_tool_prefix}file, /usr/bin:$PATH)
if test -z "$lt_cv_path_MAGIC_CMD"; then
  if test -n "$ac_tool_prefix"; then
    AC_PATH_TOOL_PREFIX(file, /usr/bin:$PATH)
  else
    MAGIC_CMD=:
  fi
fi
])


# AC_PROG_LD - find the path to the GNU or non-GNU linker
AC_DEFUN([AC_PROG_LD],
[AC_ARG_WITH(gnu-ld,
[  --with-gnu-ld           assume the C compiler uses GNU ld [default=no]],
test "$withval" = no || with_gnu_ld=yes, with_gnu_ld=no)
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
ac_prog=ld
if test "$GCC" = yes; then
  # Check if gcc -print-prog-name=ld gives a path.
  AC_MSG_CHECKING([for ld used by GCC])
  case $host in
  *-*-mingw*)
    # gcc leaves a trailing carriage return which upsets mingw
    ac_prog=`($CC -print-prog-name=ld) 2>&5 | tr -d '\015'` ;;
  *)
    ac_prog=`($CC -print-prog-name=ld) 2>&5` ;;
  esac
  case $ac_prog in
    # Accept absolute paths.
    [[\\/]* | [A-Za-z]:[\\/]*)]
      [re_direlt='/[^/][^/]*/\.\./']
      # Canonicalize the path of ld
      ac_prog=`echo $ac_prog| sed 's%\\\\%/%g'`
      while echo $ac_prog | grep "$re_direlt" > /dev/null 2>&1; do
	ac_prog=`echo $ac_prog| sed "s%$re_direlt%/%"`
      done
      test -z "$LD" && LD="$ac_prog"
      ;;
  "")
    # If it fails, then pretend we aren't using GCC.
    ac_prog=ld
    ;;
  *)
    # If it is relative, then search for the first ld in PATH.
    with_gnu_ld=unknown
    ;;
  esac
elif test "$with_gnu_ld" = yes; then
  AC_MSG_CHECKING([for GNU ld])
else
  AC_MSG_CHECKING([for non-GNU ld])
fi
AC_CACHE_VAL(lt_cv_path_LD,
[if test -z "$LD"; then
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR-:}"
  for ac_dir in $PATH; do
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog" || test -f "$ac_dir/$ac_prog$ac_exeext"; then
      lt_cv_path_LD="$ac_dir/$ac_prog"
      # Check to see if the program is GNU ld.  I'd rather use --version,
      # but apparently some GNU ld's only accept -v.
      # Break only if it was the GNU/non-GNU ld that we prefer.
      if "$lt_cv_path_LD" -v 2>&1 < /dev/null | egrep '(GNU|with BFD)' > /dev/null; then
	test "$with_gnu_ld" != no && break
      else
	test "$with_gnu_ld" != yes && break
      fi
    fi
  done
  IFS="$ac_save_ifs"
else
  lt_cv_path_LD="$LD" # Let the user override the test with a path.
fi])
LD="$lt_cv_path_LD"
if test -n "$LD"; then
  AC_MSG_RESULT($LD)
else
  AC_MSG_RESULT(no)
fi
test -z "$LD" && AC_MSG_ERROR([no acceptable ld found in \$PATH])
AC_PROG_LD_GNU
])

# AC_PROG_LD_GNU -
AC_DEFUN([AC_PROG_LD_GNU],
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], lt_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU ld's only accept -v.
if $LD -v 2>&1 </dev/null | egrep '(GNU|with BFD)' 1>&5; then
  lt_cv_prog_gnu_ld=yes
else
  lt_cv_prog_gnu_ld=no
fi])
with_gnu_ld=$lt_cv_prog_gnu_ld
])

# AC_PROG_LD_RELOAD_FLAG - find reload flag for linker
#   -- PORTME Some linkers may need a different reload flag.
AC_DEFUN([AC_PROG_LD_RELOAD_FLAG],
[AC_CACHE_CHECK([for $LD option to reload object files], lt_cv_ld_reload_flag,
[lt_cv_ld_reload_flag='-r'])
reload_flag=$lt_cv_ld_reload_flag
test -n "$reload_flag" && reload_flag=" $reload_flag"
])

# AC_DEPLIBS_CHECK_METHOD - how to check for library dependencies
#  -- PORTME fill in with the dynamic library characteristics
AC_DEFUN([AC_DEPLIBS_CHECK_METHOD],
[AC_CACHE_CHECK([how to recognise dependant libraries],
lt_cv_deplibs_check_method,
[lt_cv_file_magic_cmd='$MAGIC_CMD'
lt_cv_file_magic_test_file=
lt_cv_deplibs_check_method='unknown'
# Need to set the preceding variable on all platforms that support
# interlibrary dependencies.
# 'none' -- dependencies not supported.
# `unknown' -- same as none, but documents that we really don't know.
# 'pass_all' -- all dependencies passed with no checks.
# 'test_compile' -- check by making test program.
# ['file_magic [regex]'] -- check by looking for files in library path
# which responds to the $file_magic_cmd with a given egrep regex.
# If you have `file' or equivalent on your system and you're not sure
# whether `pass_all' will *always* work, you probably want this one.

case $host_os in
aix4* | aix5*)
  lt_cv_deplibs_check_method=pass_all
  ;;

beos*)
  lt_cv_deplibs_check_method=pass_all
  ;;

bsdi4*)
  [lt_cv_deplibs_check_method='file_magic ELF [0-9][0-9]*-bit [ML]SB (shared object|dynamic lib)']
  lt_cv_file_magic_cmd='/usr/bin/file -L'
  lt_cv_file_magic_test_file=/shlib/libc.so
  ;;

cygwin* | mingw* | pw32*)
  lt_cv_deplibs_check_method='file_magic file format pei*-i386(.*architecture: i386)?'
  lt_cv_file_magic_cmd='$OBJDUMP -f'
  ;;

darwin* | rhapsody*)
  lt_cv_deplibs_check_method='file_magic Mach-O dynamically linked shared library'
  lt_cv_file_magic_cmd='/usr/bin/file -L'
  case "$host_os" in
  rhapsody* | darwin1.[012])
    lt_cv_file_magic_test_file=`echo /System/Library/Frameworks/System.framework/Versions/*/System | head -1`
    ;;
  *) # Darwin 1.3 on
    lt_cv_file_magic_test_file='/usr/lib/libSystem.dylib'
    ;;
  esac
  ;;

freebsd*)
  if echo __ELF__ | $CC -E - | grep __ELF__ > /dev/null; then
    case $host_cpu in
    i*86 )
      # Not sure whether the presence of OpenBSD here was a mistake.
      # Let's accept both of them until this is cleared up.
      [lt_cv_deplibs_check_method='file_magic (FreeBSD|OpenBSD)/i[3-9]86 (compact )?demand paged shared library']
      lt_cv_file_magic_cmd=/usr/bin/file
      lt_cv_file_magic_test_file=`echo /usr/lib/libc.so.*`
      ;;
    esac
  else
    lt_cv_deplibs_check_method=pass_all
  fi
  ;;

gnu*)
  lt_cv_deplibs_check_method=pass_all
  ;;

hpux10.20*|hpux11*)
  [lt_cv_deplibs_check_method='file_magic (s[0-9][0-9][0-9]|PA-RISC[0-9].[0-9]) shared library']
  lt_cv_file_magic_cmd=/usr/bin/file
  lt_cv_file_magic_test_file=/usr/lib/libc.sl
  ;;

irix5* | irix6*)
  case $host_os in
  irix5*)
    # this will be overridden with pass_all, but let us keep it just in case
    lt_cv_deplibs_check_method="file_magic ELF 32-bit MSB dynamic lib MIPS - version 1"
    ;;
  *)
    case $LD in
    *-32|*"-32 ") libmagic=32-bit;;
    *-n32|*"-n32 ") libmagic=N32;;
    *-64|*"-64 ") libmagic=64-bit;;
    *) libmagic=never-match;;
    esac
    # this will be overridden with pass_all, but let us keep it just in case
    [lt_cv_deplibs_check_method="file_magic ELF ${libmagic} MSB mips-[1234] dynamic lib MIPS - version 1"]
    ;;
  esac
  lt_cv_file_magic_test_file=`echo /lib${libsuff}/libc.so*`
  lt_cv_deplibs_check_method=pass_all
  ;;

# This must be Linux ELF.
linux-gnu*)
  case $host_cpu in
  alpha* | i*86 | powerpc* | sparc* | ia64* )
    lt_cv_deplibs_check_method=pass_all ;;
  *)
    # glibc up to 2.1.1 does not perform some relocations on ARM
    [lt_cv_deplibs_check_method='file_magic ELF [0-9][0-9]*-bit [LM]SB (shared object|dynamic lib )' ;;]
  esac
  lt_cv_file_magic_test_file=`echo /lib/libc.so* /lib/libc-*.so`
  ;;

netbsd*)
  if echo __ELF__ | $CC -E - | grep __ELF__ > /dev/null; then
    [lt_cv_deplibs_check_method='match_pattern /lib[^/\.]+\.so\.[0-9]+\.[0-9]+$']
  else
    [lt_cv_deplibs_check_method='match_pattern /lib[^/\.]+\.so$']
  fi
  ;;

newos6*)
  [lt_cv_deplibs_check_method='file_magic ELF [0-9][0-9]*-bit [ML]SB (executable|dynamic lib)']
  lt_cv_file_magic_cmd=/usr/bin/file
  lt_cv_file_magic_test_file=/usr/lib/libnls.so
  ;;

osf3* | osf4* | osf5*)
  # this will be overridden with pass_all, but let us keep it just in case
  lt_cv_deplibs_check_method='file_magic COFF format alpha shared library'
  lt_cv_file_magic_test_file=/shlib/libc.so
  lt_cv_deplibs_check_method=pass_all
  ;;

sco3.2v5*)
  lt_cv_deplibs_check_method=pass_all
  ;;

solaris*)
  lt_cv_deplibs_check_method=pass_all
  lt_cv_file_magic_test_file=/lib/libc.so
  ;;

[sysv5uw[78]* | sysv4*uw2*)]
  lt_cv_deplibs_check_method=pass_all
  ;;

sysv4 | sysv4.2uw2* | sysv4.3* | sysv5*)
  case $host_vendor in
  motorola)
    [lt_cv_deplibs_check_method='file_magic ELF [0-9][0-9]*-bit [ML]SB (shared object|dynamic lib) M[0-9][0-9]* Version [0-9]']
    lt_cv_file_magic_test_file=`echo /usr/lib/libc.so*`
    ;;
  ncr)
    lt_cv_deplibs_check_method=pass_all
    ;;
  sequent)
    lt_cv_file_magic_cmd='/bin/file'
    [lt_cv_deplibs_check_method='file_magic ELF [0-9][0-9]*-bit [LM]SB (shared object|dynamic lib )']
    ;;
  sni)
    lt_cv_file_magic_cmd='/bin/file'
    [lt_cv_deplibs_check_method="file_magic ELF [0-9][0-9]*-bit [LM]SB dynamic lib"]
    lt_cv_file_magic_test_file=/lib/libc.so
    ;;
  esac
  ;;
esac
])
file_magic_cmd=$lt_cv_file_magic_cmd
deplibs_check_method=$lt_cv_deplibs_check_method
])


# AC_PROG_NM - find the path to a BSD-compatible name lister
AC_DEFUN([AC_PROG_NM],
[AC_MSG_CHECKING([for BSD-compatible nm])
AC_CACHE_VAL(lt_cv_path_NM,
[if test -n "$NM"; then
  # Let the user override the test.
  lt_cv_path_NM="$NM"
else
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR-:}"
  for ac_dir in $PATH /usr/ccs/bin /usr/ucb /bin; do
    test -z "$ac_dir" && ac_dir=.
    tmp_nm=$ac_dir/${ac_tool_prefix}nm
    if test -f $tmp_nm || test -f $tmp_nm$ac_exeext ; then
      # Check to see if the nm accepts a BSD-compat flag.
      # Adding the `sed 1q' prevents false positives on HP-UX, which says:
      #   nm: unknown option "B" ignored
      # Tru64's nm complains that /dev/null is an invalid object file
      if ($tmp_nm -B /dev/null 2>&1 | sed '1q'; exit 0) | egrep '(/dev/null|Invalid file or object type)' >/dev/null; then
	lt_cv_path_NM="$tmp_nm -B"
	break
      elif ($tmp_nm -p /dev/null 2>&1 | sed '1q'; exit 0) | egrep /dev/null >/dev/null; then
	lt_cv_path_NM="$tmp_nm -p"
	break
      else
	lt_cv_path_NM=${lt_cv_path_NM="$tmp_nm"} # keep the first match, but
	continue # so that we can try to find one that supports BSD flags
      fi
    fi
  done
  IFS="$ac_save_ifs"
  test -z "$lt_cv_path_NM" && lt_cv_path_NM=nm
fi])
NM="$lt_cv_path_NM"
AC_MSG_RESULT([$NM])
])

# AC_CHECK_LIBM - check for math library
AC_DEFUN([AC_CHECK_LIBM],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
LIBM=
case $host in
*-*-beos* | *-*-cygwin* | *-*-pw32*)
  # These system don't have libm
  ;;
*-ncr-sysv4.3*)
  AC_CHECK_LIB(mw, _mwvalidcheckl, LIBM="-lmw")
  AC_CHECK_LIB(m, main, LIBM="$LIBM -lm")
  ;;
*)
  AC_CHECK_LIB(m, main, LIBM="-lm")
  ;;
esac
])

# AC_LIBLTDL_CONVENIENCE[(dir)] - sets LIBLTDL to the link flags for
# the libltdl convenience library and INCLTDL to the include flags for
# the libltdl header and adds --enable-ltdl-convenience to the
# configure arguments.  Note that LIBLTDL and INCLTDL are not
# AC_SUBSTed, nor is AC_CONFIG_SUBDIRS called.  If DIR is not
# provided, it is assumed to be `libltdl'.  LIBLTDL will be prefixed
# with '${top_builddir}/' and INCLTDL will be prefixed with
# '${top_srcdir}/' (note the single quotes!).  If your package is not
# flat and you're not using automake, define top_builddir and
# top_srcdir appropriately in the Makefiles.
AC_DEFUN([AC_LIBLTDL_CONVENIENCE],
[AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
  case $enable_ltdl_convenience in
  no) AC_MSG_ERROR([this package needs a convenience libltdl]) ;;
  "") enable_ltdl_convenience=yes
      ac_configure_args="$ac_configure_args --enable-ltdl-convenience" ;;
  esac
  LIBLTDL='${top_builddir}/'ifelse($#,1,[$1],['libltdl'])/libltdlc.la
  INCLTDL='-I${top_srcdir}/'ifelse($#,1,[$1],['libltdl'])
])

# AC_LIBLTDL_INSTALLABLE[(dir)] - sets LIBLTDL to the link flags for
# the libltdl installable library and INCLTDL to the include flags for
# the libltdl header and adds --enable-ltdl-install to the configure
# arguments.  Note that LIBLTDL and INCLTDL are not AC_SUBSTed, nor is
# AC_CONFIG_SUBDIRS called.  If DIR is not provided and an installed
# libltdl is not found, it is assumed to be `libltdl'.  LIBLTDL will
# be prefixed with '${top_builddir}/' and INCLTDL will be prefixed
# with '${top_srcdir}/' (note the single quotes!).  If your package is
# not flat and you're not using automake, define top_builddir and
# top_srcdir appropriately in the Makefiles.
# In the future, this macro may have to be called after AC_PROG_LIBTOOL.
AC_DEFUN([AC_LIBLTDL_INSTALLABLE],
[AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
  AC_CHECK_LIB(ltdl, main,
  [test x"$enable_ltdl_install" != xyes && enable_ltdl_install=no],
  [if test x"$enable_ltdl_install" = xno; then
     AC_MSG_WARN([libltdl not installed, but installation disabled])
   else
     enable_ltdl_install=yes
   fi
  ])
  if test x"$enable_ltdl_install" = x"yes"; then
    ac_configure_args="$ac_configure_args --enable-ltdl-install"
    LIBLTDL='${top_builddir}/'ifelse($#,1,[$1],['libltdl'])/libltdl.la
    INCLTDL='-I${top_srcdir}/'ifelse($#,1,[$1],['libltdl'])
  else
    ac_configure_args="$ac_configure_args --enable-ltdl-install=no"
    LIBLTDL="-lltdl"
    INCLTDL=
  fi
])

# old names
AC_DEFUN([AM_PROG_LIBTOOL],   [AC_PROG_LIBTOOL])
AC_DEFUN([AM_ENABLE_SHARED],  [AC_ENABLE_SHARED($@)])
AC_DEFUN([AM_ENABLE_STATIC],  [AC_ENABLE_STATIC($@)])
AC_DEFUN([AM_DISABLE_SHARED], [AC_DISABLE_SHARED($@)])
AC_DEFUN([AM_DISABLE_STATIC], [AC_DISABLE_STATIC($@)])
AC_DEFUN([AM_PROG_LD],        [AC_PROG_LD])
AC_DEFUN([AM_PROG_NM],        [AC_PROG_NM])

# This is just to silence aclocal about the macro not being used
ifelse([AC_DISABLE_FAST_INSTALL])

dnl $KTH: wflags.m4,v 1.3 1999/03/11 12:11:41 joda Exp $
dnl
dnl set WFLAGS

AC_DEFUN(AC_WFLAGS,[
WFLAGS_NOUNUSED=""
WFLAGS_NOIMPLICITINT=""
if test -z "$WFLAGS" -a "$GCC" = "yes"; then
  # -Wno-implicit-int for broken X11 headers
  # leave these out for now:
  #   -Wcast-align doesn't work well on alpha osf/1
  #   -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast
  #   -Wmissing-declarations -Wnested-externs
  WFLAGS="ifelse($#, 0,-Wall, $1)"
  WFLAGS_NOUNUSED="-Wno-unused"
  WFLAGS_NOIMPLICITINT="-Wno-implicit-int"
fi
AC_SUBST(WFLAGS)dnl
AC_SUBST(WFLAGS_NOUNUSED)dnl
AC_SUBST(WFLAGS_NOIMPLICITINT)dnl
])

dnl $KTH: db.m4,v 1.4 2001/09/03 04:26:18 assar Exp $
dnl
dnl tests for various db libraries
dnl
AC_DEFUN([rk_DB],[berkeley_db=db
AC_ARG_WITH(berkeley-db,
[  --without-berkeley-db   if you don't want berkeley db],[
if test "$withval" = no; then
	berkeley_db=""
fi
])

db_type=unknown

if test "$berkeley_db"; then

  AC_CHECK_HEADERS([				\
	db.h					\
	db_185.h				\
  ])

dnl db_create is used by db3

  AC_FIND_FUNC_NO_LIBS(db_create, $berkeley_db, [
  #include <stdio.h>
  #include <db.h>
  ],[NULL, NULL, 0])

  if test "$ac_cv_func_db_create" = "yes"; then
    db_type=db3
    if test "$ac_cv_funclib_db_create" != "yes"; then
      DBLIB="$ac_cv_funclib_db_create"
    else
      DBLIB=""
    fi
    AC_DEFINE(HAVE_DB3, 1, [define if you have a berkeley db3 library])
  else

dnl dbopen is used by db1/db2

    AC_FIND_FUNC_NO_LIBS(dbopen, $berkeley_db, [
    #include <stdio.h>
    #if defined(HAVE_DB_185_H)
    #include <db_185.h>
    #elif defined(HAVE_DB_H)
    #include <db.h>
    #else
    #error no db.h
    #endif
    ],[NULL, 0, 0, 0, NULL])

    if test "$ac_cv_func_dbopen" = "yes"; then
      db_type=db1
      if test "$ac_cv_funclib_dbopen" != "yes"; then
        DBLIB="$ac_cv_funclib_dbopen"
      else
        DBLIB=""
      fi
      AC_DEFINE(HAVE_DB1, 1, [define if you have a berkeley db1/2 library])
    fi
  fi

dnl test for ndbm compatability

  AC_FIND_FUNC_NO_LIBS2(dbm_firstkey, $ac_cv_funclib_dbopen $ac_cv_funclib_db_create, [
  #include <stdio.h>
  #define DB_DBM_HSEARCH 1
  #include <db.h>
  DBM *dbm;
  ],[NULL])

  if test "$ac_cv_func_dbm_firstkey" = "yes"; then
    if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
      LIB_NDBM="$ac_cv_funclib_dbm_firstkey"
    else
      LIB_NDBM=""
    fi
    AC_DEFINE(HAVE_DB_NDBM, 1, [define if you have ndbm compat in db])
  else
    $as_unset ac_cv_func_dbm_firstkey
    $as_unset ac_cv_funclib_dbm_firstkey
  fi

fi # berkeley db

if test "$db_type" = "unknown" -o "$ac_cv_func_dbm_firstkey" = ""; then

  AC_CHECK_HEADERS([				\
	dbm.h					\
	ndbm.h					\
  ])

  AC_FIND_FUNC_NO_LIBS(dbm_firstkey, ndbm, [
  #include <stdio.h>
  #if defined(HAVE_NDBM_H)
  #include <ndbm.h>
  #elif defined(HAVE_DBM_H)
  #include <dbm.h>
  #else
  #error no ndbm.h
  #endif
  DBM *dbm;
  ],[NULL])

  if test "$ac_cv_func_dbm_firstkey" = "yes"; then
    if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
      LIB_NDBM="$ac_cv_funclib_dbm_firstkey"
    else
      LIB_NDBM=""
    fi
    AC_DEFINE(HAVE_NDBM, 1, [define if you have a ndbm library])dnl
    if test "$db_type" = "unknown"; then
      db_type=ndbm
      DBLIB="$LIB_NDBM"
    fi
  else

    $as_unset ac_cv_func_dbm_firstkey
    $as_unset ac_cv_funclib_dbm_firstkey

    AC_CHECK_HEADERS([				\
	  gdbm/ndbm.h				\
    ])

    AC_FIND_FUNC_NO_LIBS(dbm_firstkey, gdbm, [
    #include <stdio.h>
    #include <gdbm/ndbm.h>
    DBM *dbm;
    ],[NULL])

    if test "$ac_cv_func_dbm_firstkey" = "yes"; then
      if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
	LIB_NDBM="$ac_cv_funclib_dbm_firstkey"
      else
	LIB_NDBM=""
      fi
      AC_DEFINE(HAVE_NDBM, 1, [define if you have a ndbm library])dnl
      if test "$db_type" = "unknown"; then
	db_type=ndbm
	DBLIB="$LIB_NDBM"
      fi
    fi
  fi

fi # unknown
AC_SUBST(DBLIB)dnl
AC_SUBST(LIB_NDBM)dnl
])

dnl $KTH: find-func-no-libs.m4,v 1.5 1999/10/30 21:08:18 assar Exp $
dnl
dnl
dnl Look for function in any of the specified libraries
dnl

dnl AC_FIND_FUNC_NO_LIBS(func, libraries, includes, arguments, extra libs, extra args)
AC_DEFUN(AC_FIND_FUNC_NO_LIBS, [
AC_FIND_FUNC_NO_LIBS2([$1], ["" $2], [$3], [$4], [$5], [$6])])

dnl $KTH: find-func-no-libs2.m4,v 1.6 2001/09/01 10:57:32 assar Exp $
dnl
dnl
dnl Look for function in any of the specified libraries
dnl

dnl AC_FIND_FUNC_NO_LIBS2(func, libraries, includes, arguments, extra libs, extra args)
AC_DEFUN(AC_FIND_FUNC_NO_LIBS2, [

AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_funclib_$1,
[
if eval "test \"\$ac_cv_func_$1\" != yes" ; then
	ac_save_LIBS="$LIBS"
	for ac_lib in $2; do
		case "$ac_lib" in
		"") ;;
		yes) ac_lib="" ;;
		no) continue ;;
		-l*) ;;
		*) ac_lib="-l$ac_lib" ;;
		esac
		LIBS="$6 $ac_lib $5 $ac_save_LIBS"
		AC_TRY_LINK([$3],[$1($4)],eval "if test -n \"$ac_lib\";then ac_cv_funclib_$1=$ac_lib; else ac_cv_funclib_$1=yes; fi";break)
	done
	eval "ac_cv_funclib_$1=\${ac_cv_funclib_$1-no}"
	LIBS="$ac_save_LIBS"
fi
])

eval "ac_res=\$ac_cv_funclib_$1"

if false; then
	AC_CHECK_FUNCS($1)
dnl	AC_CHECK_LIBS($2, foo)
fi
# $1
eval "ac_tr_func=HAVE_[]upcase($1)"
eval "ac_tr_lib=HAVE_LIB[]upcase($ac_res | sed -e 's/-l//')"
eval "LIB_$1=$ac_res"

case "$ac_res" in
	yes)
	eval "ac_cv_func_$1=yes"
	eval "LIB_$1="
	AC_DEFINE_UNQUOTED($ac_tr_func)
	AC_MSG_RESULT([yes])
	;;
	no)
	eval "ac_cv_func_$1=no"
	eval "LIB_$1="
	AC_MSG_RESULT([no])
	;;
	*)
	eval "ac_cv_func_$1=yes"
	eval "ac_cv_lib_`echo "$ac_res" | sed 's/-l//'`=yes"
	AC_DEFINE_UNQUOTED($ac_tr_func)
	AC_DEFINE_UNQUOTED($ac_tr_lib)
	AC_MSG_RESULT([yes, in $ac_res])
	;;
esac
AC_SUBST(LIB_$1)
])

dnl $KTH: roken-frag.m4,v 1.30 2001/08/27 23:06:50 assar Exp $
dnl
dnl some code to get roken working
dnl
dnl rk_ROKEN(subdir)
dnl
AC_DEFUN(rk_ROKEN, [

AC_REQUIRE([rk_CONFIG_HEADER])

DIR_roken=roken
LIB_roken='$(top_builddir)/$1/libroken.la'
INCLUDES_roken='-I$(top_builddir)/$1 -I$(top_srcdir)/$1'

dnl Checks for programs
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AC_PROG_AWK])
AC_REQUIRE([AC_OBJEXT])
AC_REQUIRE([AC_EXEEXT])
AC_REQUIRE([AC_PROG_LIBTOOL])

AC_REQUIRE([AC_MIPS_ABI])

dnl C characteristics

AC_REQUIRE([AC_C___ATTRIBUTE__])
AC_REQUIRE([AC_C_INLINE])
AC_REQUIRE([AC_C_CONST])
AC_WFLAGS(-Wall -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast -Wmissing-declarations -Wnested-externs)

AC_REQUIRE([rk_DB])

dnl C types

AC_REQUIRE([AC_TYPE_SIZE_T])
AC_CHECK_TYPE_EXTRA(ssize_t, int, [#include <unistd.h>])
AC_REQUIRE([AC_TYPE_PID_T])
AC_REQUIRE([AC_TYPE_UID_T])
AC_HAVE_TYPE([long long])

AC_REQUIRE([rk_RETSIGTYPE])

dnl Checks for header files.
AC_REQUIRE([AC_HEADER_STDC])
AC_REQUIRE([AC_HEADER_TIME])

AC_CHECK_HEADERS([\
	arpa/inet.h				\
	arpa/nameser.h				\
	config.h				\
	crypt.h					\
	dirent.h				\
	errno.h					\
	err.h					\
	fcntl.h					\
	grp.h					\
	ifaddrs.h				\
	net/if.h				\
	netdb.h					\
	netinet/in.h				\
	netinet/in6.h				\
	netinet/in_systm.h			\
	netinet6/in6.h				\
	netinet6/in6_var.h			\
	paths.h					\
	pwd.h					\
	resolv.h				\
	rpcsvc/ypclnt.h				\
	shadow.h				\
	sys/bswap.h				\
	sys/ioctl.h				\
	sys/param.h				\
	sys/proc.h				\
	sys/resource.h				\
	sys/socket.h				\
	sys/sockio.h				\
	sys/stat.h				\
	sys/sysctl.h				\
	sys/time.h				\
	sys/tty.h				\
	sys/types.h				\
	sys/uio.h				\
	sys/utsname.h				\
	sys/wait.h				\
	syslog.h				\
	termios.h				\
	unistd.h				\
	userconf.h				\
	usersec.h				\
	util.h					\
	vis.h					\
])
	
AC_REQUIRE([CHECK_NETINET_IP_AND_TCP])

AM_CONDITIONAL(have_err_h, test "$ac_cv_header_err_h" = yes)
AM_CONDITIONAL(have_fnmatch_h, test "$ac_cv_header_fnmatch_h" = yes)
AM_CONDITIONAL(have_ifaddrs_h, test "$ac_cv_header_ifaddrs_h" = yes)
AM_CONDITIONAL(have_vis_h, test "$ac_cv_header_vis_h" = yes)

dnl Check for functions and libraries

AC_KRB_IPV6

AC_FIND_FUNC(socket, socket)
AC_FIND_FUNC(gethostbyname, nsl)
AC_FIND_FUNC(syslog, syslog)
AC_FIND_FUNC(gethostbyname2, inet6 ip6)

AC_FIND_FUNC(res_search, resolv,
[
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
],
[0,0,0,0,0])

AC_FIND_FUNC(dn_expand, resolv,
[
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
],
[0,0,0,0,0])

AC_BROKEN_SNPRINTF
AC_BROKEN_VSNPRINTF

AC_BROKEN_GLOB
if test "$ac_cv_func_glob_working" != yes; then
	LIBOBJS="$LIBOBJS glob.o"
fi
AM_CONDITIONAL(have_glob_h, test "$ac_cv_func_glob_working" = yes)


AC_CHECK_FUNCS([				\
	asnprintf				\
	asprintf				\
	cgetent					\
	getconfattr				\
	getprogname				\
	getrlimit				\
	getspnam				\
	initstate				\
	issetugid				\
	random					\
	setprogname				\
	setstate				\
	strsvis					\
	strunvis				\
	strvis					\
	strvisx					\
	svis					\
	sysconf					\
	sysctl					\
	uname					\
	unvis					\
	vasnprintf				\
	vasprintf				\
	vis					\
])

if test "$ac_cv_func_cgetent" = no; then
	LIBOBJS="$LIBOBJS getcap.o"
fi

AC_REQUIRE([AC_FUNC_GETLOGIN])

AC_FIND_FUNC_NO_LIBS(getsockopt,,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif],
[0,0,0,0,0])
AC_FIND_FUNC_NO_LIBS(setsockopt,,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif],
[0,0,0,0,0])

AC_FIND_IF_NOT_BROKEN(hstrerror, resolv,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],
17)
if test "$ac_cv_func_hstrerror" = yes; then
AC_NEED_PROTO([
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],
hstrerror)
fi

dnl sigh, wish this could be done in a loop
if test "$ac_cv_func_asprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
asprintf)dnl
fi
if test "$ac_cv_func_vasprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
vasprintf)dnl
fi
if test "$ac_cv_func_asnprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
asnprintf)dnl
fi
if test "$ac_cv_func_vasnprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
vasnprintf)dnl
fi

AC_FIND_FUNC_NO_LIBS(bswap16,,
[#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif],0)

AC_FIND_FUNC_NO_LIBS(bswap32,,
[#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif],0)

AC_FIND_FUNC_NO_LIBS(pidfile,util,
[#ifdef HAVE_UTIL_H
#include <util.h>
#endif],0)

AC_FIND_IF_NOT_BROKEN(getaddrinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0,0,0,0])

AC_FIND_IF_NOT_BROKEN(getnameinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0,0,0,0,0,0,0])

AC_FIND_IF_NOT_BROKEN(freeaddrinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0])

AC_FIND_IF_NOT_BROKEN(gai_strerror,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0])

AC_BROKEN([					\
	chown					\
	copyhostent				\
	daemon					\
	ecalloc					\
	emalloc					\
	erealloc				\
	estrdup					\
	err					\
	errx					\
	fchown					\
	flock					\
	fnmatch					\
	freehostent				\
	getcwd					\
	getdtablesize				\
	getegid					\
	geteuid					\
	getgid					\
	gethostname				\
	getifaddrs				\
	getipnodebyaddr				\
	getipnodebyname				\
	getopt					\
	gettimeofday				\
	getuid					\
	getusershell				\
	initgroups				\
	innetgr					\
	iruserok				\
	lstat					\
	memmove					\
	mkstemp					\
	putenv					\
	rcmd					\
	readv					\
	recvmsg					\
	sendmsg					\
	setegid					\
	setenv					\
	seteuid					\
	strcasecmp				\
	strdup					\
	strerror				\
	strftime				\
	strlcat					\
	strlcpy					\
	strlwr					\
	strncasecmp				\
	strndup					\
	strnlen					\
	strptime				\
	strsep					\
	strsep_copy				\
	strtok_r				\
	strupr					\
	swab					\
	unsetenv				\
	verr					\
	verrx					\
	vsyslog					\
	vwarn					\
	vwarnx					\
	warn					\
	warnx					\
	writev					\
])

AC_BROKEN2(inet_aton,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
[0,0])

AC_BROKEN2(inet_ntop,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
[0, 0, 0, 0])

AC_BROKEN2(inet_pton,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
[0,0,0])

dnl
dnl Check for sa_len in struct sockaddr, 
dnl needs to come before the getnameinfo test
dnl
AC_HAVE_STRUCT_FIELD(struct sockaddr, sa_len, [#include <sys/types.h>
#include <sys/socket.h>])

if test "$ac_cv_func_getnameinfo" = "yes"; then
  rk_BROKEN_GETNAMEINFO
  if test "$ac_cv_func_getnameinfo_broken" = yes; then
    LIBOBJS="$LIBOBJS getnameinfo.o"
  fi
fi

if test "$ac_cv_func_getaddrinfo" = "yes"; then
  rk_BROKEN_GETADDRINFO
  if test "$ac_cv_func_getaddrinfo_numserv" = no; then
    LIBOBJS="$LIBOBJS getaddrinfo.o"
  fi
fi

AC_NEED_PROTO([#include <stdlib.h>], setenv)
AC_NEED_PROTO([#include <stdlib.h>], unsetenv)
AC_NEED_PROTO([#include <unistd.h>], gethostname)
AC_NEED_PROTO([#include <unistd.h>], mkstemp)
AC_NEED_PROTO([#include <unistd.h>], getusershell)

AC_NEED_PROTO([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
inet_aton)

AC_FIND_FUNC_NO_LIBS(crypt, crypt)dnl

AC_REQUIRE([rk_BROKEN_REALLOC])dnl

dnl AC_KRB_FUNC_GETCWD_BROKEN

dnl
dnl Checks for prototypes and declarations
dnl

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
],
gethostbyname, struct hostent *gethostbyname(const char *))

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
],
gethostbyaddr, struct hostent *gethostbyaddr(const void *, size_t, int))

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
],
getservbyname, struct servent *getservbyname(const char *, const char *))

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
],
getsockname, int getsockname(int, struct sockaddr*, socklen_t*))

AC_PROTO_COMPAT([
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
],
openlog, void openlog(const char *, int, int))

AC_NEED_PROTO([
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
],
crypt)

AC_NEED_PROTO([
#include <string.h>
],
strtok_r)

AC_NEED_PROTO([
#include <string.h>
],
strsep)

dnl variables

rk_CHECK_VAR(h_errno, 
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

rk_CHECK_VAR(h_errlist, 
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

rk_CHECK_VAR(h_nerr, 
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

rk_CHECK_VAR([__progname], 
[#ifdef HAVE_ERR_H
#include <err.h>
#endif])

AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], optarg)
AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], optind)
AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], opterr)
AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], optopt)

AC_CHECK_DECLARATION([#include <stdlib.h>], environ)

dnl
dnl Check for fields in struct tm
dnl

AC_HAVE_STRUCT_FIELD(struct tm, tm_gmtoff, [#include <time.h>])
AC_HAVE_STRUCT_FIELD(struct tm, tm_zone, [#include <time.h>])

dnl
dnl or do we have a variable `timezone' ?
dnl

rk_CHECK_VAR(timezone,[#include <time.h>])

AC_HAVE_TYPE([sa_family_t],[#include <sys/socket.h>])
AC_HAVE_TYPE([socklen_t],[#include <sys/socket.h>])
AC_HAVE_TYPE([struct sockaddr], [#include <sys/socket.h>])
AC_HAVE_TYPE([struct sockaddr_storage], [#include <sys/socket.h>])
AC_HAVE_TYPE([struct addrinfo], [#include <netdb.h>])
AC_HAVE_TYPE([struct ifaddrs], [#include <ifaddrs.h>])

dnl
dnl Check for struct winsize
dnl

AC_KRB_STRUCT_WINSIZE

dnl
dnl Check for struct spwd
dnl

AC_KRB_STRUCT_SPWD

dnl won't work with automake
dnl moved to AC_OUTPUT in configure.in
dnl AC_CONFIG_FILES($1/Makefile)

LIB_roken="${LIB_roken} \$(LIB_crypt) \$(LIB_dbopen)"

AC_SUBST(DIR_roken)dnl
AC_SUBST(LIB_roken)dnl
AC_SUBST(INCLUDES_roken)dnl
])

dnl $KTH: check-type-extra.m4,v 1.2 1999/03/01 09:52:23 joda Exp $
dnl
dnl ac_check_type + extra headers

dnl AC_CHECK_TYPE_EXTRA(TYPE, DEFAULT, HEADERS)
AC_DEFUN(AC_CHECK_TYPE_EXTRA,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_type_$1,
[AC_EGREP_CPP(dnl
changequote(<<,>>)dnl
<<$1[^a-zA-Z_0-9]>>dnl
changequote([,]), [#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
$3], ac_cv_type_$1=yes, ac_cv_type_$1=no)])dnl
AC_MSG_RESULT($ac_cv_type_$1)
if test $ac_cv_type_$1 = no; then
  AC_DEFINE($1, $2, [Define this to what the type $1 should be.])
fi
])

dnl $KTH: have-type.m4,v 1.6 2000/07/15 18:10:00 joda Exp $
dnl
dnl check for existance of a type

dnl AC_HAVE_TYPE(TYPE,INCLUDES)
AC_DEFUN(AC_HAVE_TYPE, [
AC_REQUIRE([AC_HEADER_STDC])
cv=`echo "$1" | sed 'y%./+- %__p__%'`
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL([ac_cv_type_$cv],
AC_TRY_COMPILE(
[#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
$2],
[$1 foo;],
eval "ac_cv_type_$cv=yes",
eval "ac_cv_type_$cv=no"))dnl
ac_foo=`eval echo \\$ac_cv_type_$cv`
AC_MSG_RESULT($ac_foo)
if test "$ac_foo" = yes; then
  ac_tr_hdr=HAVE_`echo $1 | sed 'y%abcdefghijklmnopqrstuvwxyz./- %ABCDEFGHIJKLMNOPQRSTUVWXYZ____%'`
if false; then
	AC_CHECK_TYPES($1)
fi
  AC_DEFINE_UNQUOTED($ac_tr_hdr, 1, [Define if you have type `$1'])
fi
])

dnl
dnl $KTH: retsigtype.m4,v 1.1 2000/07/15 18:05:56 joda Exp $
dnl
dnl Figure out return type of signal handlers, and define SIGRETURN macro
dnl that can be used to return from one
dnl
AC_DEFUN(rk_RETSIGTYPE,[
AC_TYPE_SIGNAL
if test "$ac_cv_type_signal" = "void" ; then
	AC_DEFINE(VOID_RETSIGTYPE, 1, [Define if signal handlers return void.])
fi
AC_SUBST(VOID_RETSIGTYPE)
AH_BOTTOM([#ifdef VOID_RETSIGTYPE
#define SIGRETURN(x) return
#else
#define SIGRETURN(x) return (RETSIGTYPE)(x)
#endif])
])
dnl
dnl $KTH: check-netinet-ip-and-tcp.m4,v 1.3 2000/07/18 10:33:02 joda Exp $
dnl

dnl extra magic check for netinet/{ip.h,tcp.h} because on irix 6.5.3
dnl you have to include standards.h before including these files

AC_DEFUN(CHECK_NETINET_IP_AND_TCP,
[
AC_CHECK_HEADERS(standards.h)
for i in netinet/ip.h netinet/tcp.h; do

cv=`echo "$i" | sed 'y%./+-%__p_%'`

AC_CACHE_CHECK([for $i],ac_cv_header_$cv,
[AC_TRY_CPP([\
#ifdef HAVE_STANDARDS_H
#include <standards.h>
#endif
#include <$i>
],
eval "ac_cv_header_$cv=yes",
eval "ac_cv_header_$cv=no")])
ac_res=`eval echo \\$ac_cv_header_$cv`
if test "$ac_res" = yes; then
	ac_tr_hdr=HAVE_`echo $i | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
	AC_DEFINE_UNQUOTED($ac_tr_hdr, 1)
fi
done
if false;then
	AC_CHECK_HEADERS(netinet/ip.h netinet/tcp.h)
fi
])

# Define a conditional.

AC_DEFUN([AM_CONDITIONAL],
[AC_SUBST($1_TRUE)
AC_SUBST($1_FALSE)
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])

dnl $KTH: krb-ipv6.m4,v 1.12 2001/08/19 16:27:02 joda Exp $
dnl
dnl test for IPv6
dnl
AC_DEFUN(AC_KRB_IPV6, [
AC_ARG_WITH(ipv6,
[  --without-ipv6	do not enable IPv6 support],[
if test "$withval" = "no"; then
	ac_cv_lib_ipv6=no
fi])
save_CFLAGS="${CFLAGS}"
AC_CACHE_VAL(ac_cv_lib_ipv6,
[dnl check for different v6 implementations (by itojun)
v6type=unknown
v6lib=none

AC_MSG_CHECKING([ipv6 stack type])
for i in v6d toshiba kame inria zeta linux; do
	case $i in
	v6d)
		AC_EGREP_CPP(yes, [
#include </usr/local/v6/include/sys/types.h>
#ifdef __V6D__
yes
#endif],
			[v6type=$i; v6lib=v6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-I/usr/local/v6/include $CFLAGS"])
		;;
	toshiba)
		AC_EGREP_CPP(yes, [
#include <sys/param.h>
#ifdef _TOSHIBA_INET6
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	kame)
		AC_EGREP_CPP(yes, [
#include <netinet/in.h>
#ifdef __KAME__
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	inria)
		AC_EGREP_CPP(yes, [
#include <netinet/in.h>
#ifdef IPV6_INRIA_VERSION
yes
#endif],
			[v6type=$i; CFLAGS="-DINET6 $CFLAGS"])
		;;
	zeta)
		AC_EGREP_CPP(yes, [
#include <sys/param.h>
#ifdef _ZETA_MINAMI_INET6
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	linux)
		if test -d /usr/inet6; then
			v6type=$i
			v6lib=inet6
			v6libdir=/usr/inet6
			CFLAGS="-DINET6 $CFLAGS"
		fi
		;;
	esac
	if test "$v6type" != "unknown"; then
		break
	fi
done
AC_MSG_RESULT($v6type)

if test "$v6lib" != "none"; then
	for dir in $v6libdir /usr/local/v6/lib /usr/local/lib; do
		if test -d $dir -a -f $dir/lib$v6lib.a; then
			LIBS="-L$dir -l$v6lib $LIBS"
			break
		fi
	done
fi
AC_TRY_LINK([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
],
[
 struct sockaddr_in6 sin6;
 int s;

 s = socket(AF_INET6, SOCK_DGRAM, 0);

 sin6.sin6_family = AF_INET6;
 sin6.sin6_port = htons(17);
 sin6.sin6_addr = in6addr_any;
 bind(s, (struct sockaddr *)&sin6, sizeof(sin6));
],
ac_cv_lib_ipv6=yes,
ac_cv_lib_ipv6=no)])
AC_MSG_CHECKING(for IPv6)
AC_MSG_RESULT($ac_cv_lib_ipv6)
if test "$ac_cv_lib_ipv6" = yes; then
  AC_DEFINE(HAVE_IPV6, 1, [Define if you have IPv6.])
else
  CFLAGS="${save_CFLAGS}"
fi

if test "$ac_cv_lib_ipv6" = yes; then
	AC_CACHE_CHECK([for in6addr_loopback],[ac_cv_var_in6addr_loopback],[
	AC_TRY_LINK([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif],[
struct sockaddr_in6 sin6;
sin6.sin6_addr = in6addr_loopback;
],ac_cv_var_in6addr_loopback=yes,ac_cv_var_in6addr_loopback=no)])
	if test "$ac_cv_var_in6addr_loopback" = yes; then
		AC_DEFINE(HAVE_IN6ADDR_LOOPBACK, 1, 
			[Define if you have the in6addr_loopback variable])
	fi
fi
])
dnl $KTH: find-func.m4,v 1.1 1997/12/14 15:58:58 joda Exp $
dnl
dnl AC_FIND_FUNC(func, libraries, includes, arguments)
AC_DEFUN(AC_FIND_FUNC, [
AC_FIND_FUNC_NO_LIBS([$1], [$2], [$3], [$4])
if test -n "$LIB_$1"; then
	LIBS="$LIB_$1 $LIBS"
fi
])

dnl $KTH: broken-snprintf.m4,v 1.4 2001/09/01 11:56:05 assar Exp $
dnl
AC_DEFUN(AC_BROKEN_SNPRINTF, [
AC_CACHE_CHECK(for working snprintf,ac_cv_func_snprintf_working,
ac_cv_func_snprintf_working=yes
AC_TRY_RUN([
#include <stdio.h>
#include <string.h>
int main()
{
	char foo[[3]];
	snprintf(foo, 2, "12");
	return strcmp(foo, "1");
}],:,ac_cv_func_snprintf_working=no,:))

if test "$ac_cv_func_snprintf_working" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_SNPRINTF, 1, [define if you have a working snprintf])
fi
if test "$ac_cv_func_snprintf_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>],snprintf)
fi
])

AC_DEFUN(AC_BROKEN_VSNPRINTF,[
AC_CACHE_CHECK(for working vsnprintf,ac_cv_func_vsnprintf_working,
ac_cv_func_vsnprintf_working=yes
AC_TRY_RUN([
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int foo(int num, ...)
{
	char bar[[3]];
	va_list arg;
	va_start(arg, num);
	vsnprintf(bar, 2, "%s", arg);
	va_end(arg);
	return strcmp(bar, "1");
}


int main()
{
	return foo(0, "12");
}],:,ac_cv_func_vsnprintf_working=no,:))

if test "$ac_cv_func_vsnprintf_working" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_VSNPRINTF, 1, [define if you have a working vsnprintf])
fi
if test "$ac_cv_func_vsnprintf_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>],vsnprintf)
fi
])

dnl $KTH: need-proto.m4,v 1.2 1999/03/01 09:52:24 joda Exp $
dnl
dnl
dnl Check if we need the prototype for a function
dnl

dnl AC_NEED_PROTO(includes, function)

AC_DEFUN(AC_NEED_PROTO, [
if test "$ac_cv_func_$2+set" != set -o "$ac_cv_func_$2" = yes; then
AC_CACHE_CHECK([if $2 needs a prototype], ac_cv_func_$2_noproto,
AC_TRY_COMPILE([$1],
[struct foo { int foo; } xx;
extern int $2 (struct foo*);
$2(&xx);
],
eval "ac_cv_func_$2_noproto=yes",
eval "ac_cv_func_$2_noproto=no"))
define([foo], [NEED_]translit($2, [a-z], [A-Z])[_PROTO])
if test "$ac_cv_func_$2_noproto" = yes; then
	AC_DEFINE(foo, 1, [define if the system is missing a prototype for $2()])
fi
undefine([foo])
fi
])

dnl $KTH: broken-glob.m4,v 1.4 2001/06/19 09:59:46 assar Exp $
dnl
dnl check for glob(3)
dnl
AC_DEFUN(AC_BROKEN_GLOB,[
AC_CACHE_CHECK(for working glob, ac_cv_func_glob_working,
ac_cv_func_glob_working=yes
AC_TRY_LINK([
#include <stdio.h>
#include <glob.h>],[
glob(NULL, GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE|
#ifdef GLOB_MAXPATH
GLOB_MAXPATH
#else
GLOB_LIMIT
#endif
,
NULL, NULL);
],:,ac_cv_func_glob_working=no,:))

if test "$ac_cv_func_glob_working" = yes; then
	AC_DEFINE(HAVE_GLOB, 1, [define if you have a glob() that groks 
	GLOB_BRACE, GLOB_NOCHECK, GLOB_QUOTE, GLOB_TILDE, and GLOB_LIMIT])
fi
if test "$ac_cv_func_glob_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>
#include <glob.h>],glob)
fi
])

dnl
dnl $KTH: krb-func-getlogin.m4,v 1.1 1999/07/13 17:45:30 assar Exp $
dnl
dnl test for POSIX (broken) getlogin
dnl


AC_DEFUN(AC_FUNC_GETLOGIN, [
AC_CHECK_FUNCS(getlogin setlogin)
if test "$ac_cv_func_getlogin" = yes; then
AC_CACHE_CHECK(if getlogin is posix, ac_cv_func_getlogin_posix, [
if test "$ac_cv_func_getlogin" = yes -a "$ac_cv_func_setlogin" = yes; then
	ac_cv_func_getlogin_posix=no
else
	ac_cv_func_getlogin_posix=yes
fi
])
if test "$ac_cv_func_getlogin_posix" = yes; then
	AC_DEFINE(POSIX_GETLOGIN, 1, [Define if getlogin has POSIX flavour (and not BSD).])
fi
fi
])

dnl $KTH: find-if-not-broken.m4,v 1.2 1998/03/16 22:16:27 joda Exp $
dnl
dnl
dnl Mix between AC_FIND_FUNC and AC_BROKEN
dnl

AC_DEFUN(AC_FIND_IF_NOT_BROKEN,
[AC_FIND_FUNC([$1], [$2], [$3], [$4])
if eval "test \"$ac_cv_func_$1\" != yes"; then
LIBOBJS[]="$LIBOBJS $1.o"
fi
AC_SUBST(LIBOBJS)dnl
])

dnl $KTH: broken.m4,v 1.4 2000/07/15 18:06:36 joda Exp $
dnl
dnl
dnl Same as AC _REPLACE_FUNCS, just define HAVE_func if found in normal
dnl libraries 

AC_DEFUN(AC_BROKEN,
[for ac_func in $1
do
AC_CHECK_FUNC($ac_func, [
ac_tr_func=HAVE_[]upcase($ac_func)
AC_DEFINE_UNQUOTED($ac_tr_func)],[LIBOBJS[]="$LIBOBJS ${ac_func}.o"])
if false; then
	AC_CHECK_FUNCS($1)
fi
done
AC_SUBST(LIBOBJS)dnl
])

dnl $KTH: broken2.m4,v 1.1 2000/12/15 14:27:33 assar Exp $
dnl
dnl AC_BROKEN but with more arguments

dnl AC_BROKEN2(func, includes, arguments)
AC_DEFUN(AC_BROKEN2,
[for ac_func in $1
do
AC_MSG_CHECKING([for $ac_func])
AC_CACHE_VAL(ac_cv_func_$ac_func,
[AC_TRY_LINK([$2],
[
/* The GNU C library defines this for functions which it implements
    to always fail with ENOSYS.  Some functions are actually named
    something starting with __ and the normal name is an alias.  */
#if defined (__stub_$1) || defined (__stub___$1)
choke me
#else
$ac_func($3)
#endif
], [eval "ac_cv_func_$ac_func=yes"], [eval "ac_cv_func_$ac_func=no"])])
if eval "test \"\${ac_cv_func_$ac_func}\" = yes"; then
  ac_tr_func=HAVE_[]upcase($ac_func)
  AC_DEFINE_UNQUOTED($ac_tr_func)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
  LIBOBJS[]="$LIBOBJS ${ac_func}.o"
fi
done
if false; then
	AC_CHECK_FUNCS($1)
fi
AC_SUBST(LIBOBJS)dnl
])

dnl $KTH: have-struct-field.m4,v 1.6 1999/07/29 01:44:32 assar Exp $
dnl
dnl check for fields in a structure
dnl
dnl AC_HAVE_STRUCT_FIELD(struct, field, headers)

AC_DEFUN(AC_HAVE_STRUCT_FIELD, [
define(cache_val, translit(ac_cv_type_$1_$2, [A-Z ], [a-z_]))
AC_CACHE_CHECK([for $2 in $1], cache_val,[
AC_TRY_COMPILE([$3],[$1 x; x.$2;],
cache_val=yes,
cache_val=no)])
if test "$cache_val" = yes; then
	define(foo, translit(HAVE_$1_$2, [a-z ], [A-Z_]))
	AC_DEFINE(foo, 1, [Define if $1 has field $2.])
	undefine([foo])
fi
undefine([cache_val])
])

dnl $KTH: broken-getnameinfo.m4,v 1.2 2000/12/05 09:09:00 joda Exp $
dnl
dnl test for broken AIX getnameinfo

AC_DEFUN(rk_BROKEN_GETNAMEINFO,[
AC_CACHE_CHECK([if getnameinfo is broken], ac_cv_func_getnameinfo_broken,
AC_TRY_RUN([[#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int
main(int argc, char **argv)
{
  struct sockaddr_in sin;
  char host[256];
  memset(&sin, 0, sizeof(sin));
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
  sin.sin_len = sizeof(sin);
#endif
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = 0xffffffff;
  sin.sin_port = 0;
  return getnameinfo((struct sockaddr*)&sin, sizeof(sin), host, sizeof(host),
	      NULL, 0, 0);
}
]], ac_cv_func_getnameinfo_broken=no, ac_cv_func_getnameinfo_broken=yes))])

dnl $KTH: broken-getaddrinfo.m4,v 1.2 2001/08/22 01:05:29 assar Exp $
dnl
dnl test if getaddrinfo can handle numeric services

AC_DEFUN(rk_BROKEN_GETADDRINFO,[
AC_CACHE_CHECK([if getaddrinfo handles numeric services], ac_cv_func_getaddrinfo_numserv,
AC_TRY_RUN([[#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int
main(int argc, char **argv)
{
	struct addrinfo hints, *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = PF_UNSPEC;
	if(getaddrinfo(NULL, "17", &hints, &ai) == EAI_SERVICE)
		return 1;
	return 0;
}
]], ac_cv_func_getaddrinfo_numserv=yes, ac_cv_func_getaddrinfo_numserv=no))])

dnl
dnl $KTH: broken-realloc.m4,v 1.1 2000/07/15 18:05:36 joda Exp $
dnl
dnl Test for realloc that doesn't handle NULL as first parameter
dnl
AC_DEFUN(rk_BROKEN_REALLOC, [
AC_CACHE_CHECK(if realloc if broken, ac_cv_func_realloc_broken, [
ac_cv_func_realloc_broken=no
AC_TRY_RUN([
#include <stddef.h>
#include <stdlib.h>

int main()
{
	return realloc(NULL, 17) == NULL;
}
],:, ac_cv_func_realloc_broken=yes, :)
])
if test "$ac_cv_func_realloc_broken" = yes ; then
	AC_DEFINE(BROKEN_REALLOC, 1, [Define if realloc(NULL) doesn't work.])
fi
AH_BOTTOM([#ifdef BROKEN_REALLOC
#define realloc(X, Y) isoc_realloc((X), (Y))
#define isoc_realloc(X, Y) ((X) ? realloc((X), (Y)) : malloc(Y))
#endif])
])

dnl $KTH: proto-compat.m4,v 1.3 1999/03/01 13:03:48 joda Exp $
dnl
dnl
dnl Check if the prototype of a function is compatible with another one
dnl

dnl AC_PROTO_COMPAT(includes, function, prototype)

AC_DEFUN(AC_PROTO_COMPAT, [
AC_CACHE_CHECK([if $2 is compatible with system prototype],
ac_cv_func_$2_proto_compat,
AC_TRY_COMPILE([$1],
[$3;],
eval "ac_cv_func_$2_proto_compat=yes",
eval "ac_cv_func_$2_proto_compat=no"))
define([foo], translit($2, [a-z], [A-Z])[_PROTO_COMPATIBLE])
if test "$ac_cv_func_$2_proto_compat" = yes; then
	AC_DEFINE(foo, 1, [define if prototype of $2 is compatible with
	$3])
fi
undefine([foo])
])
dnl $KTH: check-var.m4,v 1.6 2001/08/21 12:00:16 joda Exp $
dnl
dnl rk_CHECK_VAR(variable, includes)
AC_DEFUN([rk_CHECK_VAR], [
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_var_$1, [
AC_TRY_LINK([extern int $1;
int foo() { return $1; }],
	    [foo()],
	    ac_cv_var_$1=yes, ac_cv_var_$1=no)
])
ac_foo=`eval echo \\$ac_cv_var_$1`
AC_MSG_RESULT($ac_foo)
if test "$ac_foo" = yes; then
	AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_[]$1), 1, 
		[Define if you have the `]$1[' variable.])
	m4_ifval([$2], AC_CHECK_DECLARATION([$2],[$1]))
fi
])

AC_WARNING_ENABLE([obsolete])
AU_DEFUN([AC_CHECK_VAR], [rk_CHECK_VAR([$2], [$1])], [foo])
dnl $KTH: check-declaration.m4,v 1.3 1999/03/01 13:03:08 joda Exp $
dnl
dnl
dnl Check if we need the declaration of a variable
dnl

dnl AC_HAVE_DECLARATION(includes, variable)
AC_DEFUN(AC_CHECK_DECLARATION, [
AC_MSG_CHECKING([if $2 is properly declared])
AC_CACHE_VAL(ac_cv_var_$2_declaration, [
AC_TRY_COMPILE([$1
extern struct { int foo; } $2;],
[$2.foo = 1;],
eval "ac_cv_var_$2_declaration=no",
eval "ac_cv_var_$2_declaration=yes")
])

define(foo, [HAVE_]translit($2, [a-z], [A-Z])[_DECLARATION])

AC_MSG_RESULT($ac_cv_var_$2_declaration)
if eval "test \"\$ac_cv_var_$2_declaration\" = yes"; then
	AC_DEFINE(foo, 1, [define if your system declares $2])
fi
undefine([foo])
])

dnl $KTH: krb-struct-winsize.m4,v 1.3 2001/09/01 11:56:05 assar Exp $
dnl
dnl
dnl Search for struct winsize
dnl

AC_DEFUN(AC_KRB_STRUCT_WINSIZE, [
AC_MSG_CHECKING(for struct winsize)
AC_CACHE_VAL(ac_cv_struct_winsize, [
ac_cv_struct_winsize=no
for i in sys/termios.h sys/ioctl.h; do
AC_EGREP_HEADER(
struct[[ 	]]*winsize,dnl
$i, ac_cv_struct_winsize=yes; break)dnl
done
])
if test "$ac_cv_struct_winsize" = "yes"; then
  AC_DEFINE(HAVE_STRUCT_WINSIZE, 1, [define if struct winsize is declared in sys/termios.h])
fi
AC_MSG_RESULT($ac_cv_struct_winsize)
AC_EGREP_HEADER(ws_xpixel, termios.h, 
	AC_DEFINE(HAVE_WS_XPIXEL, 1, [define if struct winsize has ws_xpixel]))
AC_EGREP_HEADER(ws_ypixel, termios.h, 
	AC_DEFINE(HAVE_WS_YPIXEL, 1, [define if struct winsize has ws_ypixel]))
])

dnl $KTH: krb-struct-spwd.m4,v 1.3 1999/07/13 21:04:11 assar Exp $
dnl
dnl Test for `struct spwd'

AC_DEFUN(AC_KRB_STRUCT_SPWD, [
AC_MSG_CHECKING(for struct spwd)
AC_CACHE_VAL(ac_cv_struct_spwd, [
AC_TRY_COMPILE(
[#include <pwd.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif],
[struct spwd foo;],
ac_cv_struct_spwd=yes,
ac_cv_struct_spwd=no)
])
AC_MSG_RESULT($ac_cv_struct_spwd)

if test "$ac_cv_struct_spwd" = "yes"; then
  AC_DEFINE(HAVE_STRUCT_SPWD, 1, [define if you have struct spwd])
fi
])

dnl $KTH: test-package.m4,v 1.9 2000/12/15 04:54:24 assar Exp $
dnl
dnl AC_TEST_PACKAGE_NEW(package,headers,libraries,extra libs,default locations, conditional)

AC_DEFUN(AC_TEST_PACKAGE,[AC_TEST_PACKAGE_NEW($1,[#include <$2>],$4,,$5)])

AC_DEFUN(AC_TEST_PACKAGE_NEW,[
AC_ARG_WITH($1,
[  --with-$1=dir                use $1 in dir])
AC_ARG_WITH($1-lib,
[  --with-$1-lib=dir            use $1 libraries in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-lib])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])
AC_ARG_WITH($1-include,
[  --with-$1-include=dir        use $1 headers in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-include])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])

AC_MSG_CHECKING(for $1)

case "$with_$1" in
yes)	;;
no)	;;
"")	;;
*)	if test "$with_$1_include" = ""; then
		with_$1_include="$with_$1/include"
	fi
	if test "$with_$1_lib" = ""; then
		with_$1_lib="$with_$1/lib$abilibdirext"
	fi
	;;
esac
header_dirs=
lib_dirs=
d='$5'
for i in $d; do
	header_dirs="$header_dirs $i/include"
	lib_dirs="$lib_dirs $i/lib$abilibdirext"
done

case "$with_$1_include" in
yes) ;;
no)  ;;
*)   header_dirs="$with_$1_include $header_dirs";;
esac
case "$with_$1_lib" in
yes) ;;
no)  ;;
*)   lib_dirs="$with_$1_lib $lib_dirs";;
esac

save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
ires= lres=
for i in $header_dirs; do
	CFLAGS="-I$i $save_CFLAGS"
	AC_TRY_COMPILE([$2],,ires=$i;break)
done
for i in $lib_dirs; do
	LIBS="-L$i $3 $4 $save_LIBS"
	AC_TRY_LINK([$2],,lres=$i;break)
done
CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"

if test "$ires" -a "$lres" -a "$with_$1" != "no"; then
	$1_includedir="$ires"
	$1_libdir="$lres"
	INCLUDE_$1="-I$$1_includedir"
	LIB_$1="-L$$1_libdir $3"
	m4_ifval([$6],
		AC_DEFINE_UNQUOTED($6,1,[Define if you have the $1 package.]),
		AC_DEFINE_UNQUOTED(upcase($1),1,[Define if you have the $1 package.]))
	with_$1=yes
	AC_MSG_RESULT([headers $ires, libraries $lres])
else
	INCLUDE_$1=
	LIB_$1=
	with_$1=no
	AC_MSG_RESULT($with_$1)
fi
dnl m4_ifval([$6],
dnl 	AM_CONDITIONAL($6, test "$with_$1" = yes)
dnl 	AM_CONDITIONAL(upcase($1), test "$with_$1" = yes))
AC_SUBST(INCLUDE_$1)
AC_SUBST(LIB_$1)
])

dnl $KTH: osfc2.m4,v 1.2 1999/03/27 17:28:16 joda Exp $
dnl
dnl enable OSF C2 stuff

AC_DEFUN(AC_CHECK_OSFC2,[
AC_ARG_ENABLE(osfc2,
[  --enable-osfc2          enable some OSF C2 support])
LIB_security=
if test "$enable_osfc2" = yes; then
	AC_DEFINE(HAVE_OSFC2, 1, [Define to enable basic OSF C2 support.])
	LIB_security=-lsecurity
fi
AC_SUBST(LIB_security)
])

dnl $KTH: check-man.m4,v 1.3 2000/11/30 01:47:17 joda Exp $
dnl check how to format manual pages
dnl

AC_DEFUN(rk_CHECK_MAN,
[AC_PATH_PROG(NROFF, nroff)
AC_PATH_PROG(GROFF, groff)
AC_CACHE_CHECK(how to format man pages,ac_cv_sys_man_format,
[cat > conftest.1 << END
.Dd January 1, 1970
.Dt CONFTEST 1
.Sh NAME
.Nm conftest
.Nd
foobar
END

if test "$NROFF" ; then
	for i in "-mdoc" "-mandoc"; do
		if "$NROFF" $i conftest.1 2> /dev/null | \
			grep Jan > /dev/null 2>&1 ; then
			ac_cv_sys_man_format="$NROFF $i"
			break
		fi
	done
fi
if test "$ac_cv_sys_man_format" = "" -a "$GROFF" ; then
	for i in "-mdoc" "-mandoc"; do
		if "$GROFF" -Tascii $i conftest.1 2> /dev/null | \
			grep Jan > /dev/null 2>&1 ; then
			ac_cv_sys_man_format="$GROFF -Tascii $i"
			break
		fi
	done
fi
if test "$ac_cv_sys_man_format"; then
	ac_cv_sys_man_format="$ac_cv_sys_man_format \[$]< > \[$]@"
fi
])
if test "$ac_cv_sys_man_format"; then
	CATMAN="$ac_cv_sys_man_format"
	AC_SUBST(CATMAN)
fi
AM_CONDITIONAL(CATMAN, test "$CATMAN")
AC_CACHE_CHECK(extension of pre-formatted manual pages,ac_cv_sys_catman_ext,
[if grep _suffix /etc/man.conf > /dev/null 2>&1; then
	ac_cv_sys_catman_ext=0
else
	ac_cv_sys_catman_ext=number
fi
])
if test "$ac_cv_sys_catman_ext" = number; then
	CATMANEXT='$$section'
else
	CATMANEXT=0
fi
AC_SUBST(CATMANEXT)
])
dnl
dnl $KTH: krb-bigendian.m4,v 1.6 2000/08/19 15:37:00 assar Exp $
dnl

dnl check if this computer is little or big-endian
dnl if we can figure it out at compile-time then don't define the cpp symbol
dnl otherwise test for it and define it.  also allow options for overriding
dnl it when cross-compiling

AC_DEFUN(KRB_C_BIGENDIAN, [
AC_ARG_ENABLE(bigendian,
[  --enable-bigendian	the target is big endian],
krb_cv_c_bigendian=yes)
AC_ARG_ENABLE(littleendian,
[  --enable-littleendian	the target is little endian],
krb_cv_c_bigendian=no)
AC_CACHE_CHECK(whether byte order is known at compile time,
krb_cv_c_bigendian_compile,
[AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/param.h>],[
#if !BYTE_ORDER || !BIG_ENDIAN || !LITTLE_ENDIAN
 bogus endian macros
#endif], krb_cv_c_bigendian_compile=yes, krb_cv_c_bigendian_compile=no)])
AC_CACHE_CHECK(whether byte ordering is bigendian, krb_cv_c_bigendian,[
  if test "$krb_cv_c_bigendian_compile" = "yes"; then
    AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/param.h>],[
#if BYTE_ORDER != BIG_ENDIAN
  not big endian
#endif], krb_cv_c_bigendian=yes, krb_cv_c_bigendian=no)
  else
    AC_TRY_RUN([main () {
      /* Are we little or big endian?  From Harbison&Steele.  */
      union
      {
	long l;
	char c[sizeof (long)];
    } u;
    u.l = 1;
    exit (u.c[sizeof (long) - 1] == 1);
  }], krb_cv_c_bigendian=no, krb_cv_c_bigendian=yes,
  AC_MSG_ERROR([specify either --enable-bigendian or --enable-littleendian]))
  fi
])
if test "$krb_cv_c_bigendian" = "yes"; then
  AC_DEFINE(WORDS_BIGENDIAN, 1, [define if target is big endian])dnl
fi
if test "$krb_cv_c_bigendian_compile" = "yes"; then
  AC_DEFINE(ENDIANESS_IN_SYS_PARAM_H, 1, [define if sys/param.h defines the endiness])dnl
fi
])

dnl
dnl $KTH: aix.m4,v 1.5 2000/11/05 17:15:46 joda Exp $
dnl

AC_DEFUN(KRB_AIX,[
aix=no
case "$host" in 
*-*-aix3*)
	aix=3
	;;
*-*-aix4*)
	aix=4
	;;
esac
AM_CONDITIONAL(AIX, test "$aix" != no)dnl
AM_CONDITIONAL(AIX4, test "$aix" = 4)
aix_dynamic_afs=yes
AM_CONDITIONAL(AIX_DYNAMIC_AFS, test "$aix_dynamic_afs" = yes)dnl

AC_FIND_FUNC_NO_LIBS(dlopen, dl)

if test "$aix" != no; then
	if test "$aix_dynamic_afs" = yes; then
		if test "$ac_cv_funclib_dlopen" = yes; then
			AIX_EXTRA_KAFS=
		elif test "$ac_cv_funclib_dlopen" != no; then
			AIX_EXTRA_KAFS="$ac_cv_funclib_dlopen"
		else
			AIX_EXTRA_KAFS=-lld
		fi
	else
		AIX_EXTRA_KAFS=
	fi
fi

AM_CONDITIONAL(HAVE_DLOPEN, test "$ac_cv_funclib_dlopen" != no)dnl
AC_SUBST(AIX_EXTRA_KAFS)dnl

])
dnl
dnl $KTH: krb-irix.m4,v 1.2 2000/12/13 12:48:45 assar Exp $
dnl

dnl requires AC_CANONICAL_HOST
AC_DEFUN(KRB_IRIX,[
irix=no
case "$host_os" in
irix*) irix=yes ;;
esac
AM_CONDITIONAL(IRIX, test "$irix" != no)dnl
])

dnl 
dnl See if there is any X11 present
dnl
dnl $KTH: check-x.m4,v 1.2 1999/11/05 04:25:23 assar Exp $

AC_DEFUN(KRB_CHECK_X,[
AC_PATH_XTRA

# try to figure out if we need any additional ld flags, like -R
# and yes, the autoconf X test is utterly broken
if test "$no_x" != yes; then
	AC_CACHE_CHECK(for special X linker flags,krb_cv_sys_x_libs_rpath,[
	ac_save_libs="$LIBS"
	ac_save_cflags="$CFLAGS"
	CFLAGS="$CFLAGS $X_CFLAGS"
	krb_cv_sys_x_libs_rpath=""
	krb_cv_sys_x_libs=""
	for rflag in "" "-R" "-R " "-rpath "; do
		if test "$rflag" = ""; then
			foo="$X_LIBS"
		else
			foo=""
			for flag in $X_LIBS; do
			case $flag in
			-L*)
				foo="$foo $flag `echo $flag | sed \"s/-L/$rflag/\"`"
				;;
			*)
				foo="$foo $flag"
				;;
			esac
			done
		fi
		LIBS="$ac_save_libs $foo $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
		AC_TRY_RUN([
		#include <X11/Xlib.h>
		foo()
		{
		XOpenDisplay(NULL);
		}
		main()
		{
		return 0;
		}
		], krb_cv_sys_x_libs_rpath="$rflag"; krb_cv_sys_x_libs="$foo"; break,:)
	done
	LIBS="$ac_save_libs"
	CFLAGS="$ac_save_cflags"
	])
	X_LIBS="$krb_cv_sys_x_libs"
fi
])

dnl $KTH: check-xau.m4,v 1.3 1999/05/14 01:17:06 assar Exp $
dnl
dnl check for Xau{Read,Write}Auth and XauFileName
dnl
AC_DEFUN(AC_CHECK_XAU,[
save_CFLAGS="$CFLAGS"
CFLAGS="$X_CFLAGS $CFLAGS"
save_LIBS="$LIBS"
dnl LIBS="$X_LIBS $X_PRE_LIBS $X_EXTRA_LIBS $LIBS"
LIBS="$X_PRE_LIBS $X_EXTRA_LIBS $LIBS"
save_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $X_LIBS"


AC_FIND_FUNC_NO_LIBS(XauWriteAuth, X11 Xau)
ac_xxx="$LIBS"
LIBS="$LIB_XauWriteAuth $LIBS"
AC_FIND_FUNC_NO_LIBS(XauReadAuth, X11 Xau)
LIBS="$LIB_XauReadAauth $LIBS"
AC_FIND_FUNC_NO_LIBS(XauFileName, X11 Xau)
LIBS="$ac_xxx"

case "$ac_cv_funclib_XauWriteAuth" in
yes)	;;
no)	;;
*)	if test "$ac_cv_funclib_XauReadAuth" = yes; then
		if test "$ac_cv_funclib_XauFileName" = yes; then
			LIB_XauReadAuth="$LIB_XauWriteAuth"
		else
			LIB_XauReadAuth="$LIB_XauWriteAuth $LIB_XauFileName"
		fi
	else
		if test "$ac_cv_funclib_XauFileName" = yes; then
			LIB_XauReadAuth="$LIB_XauReadAuth $LIB_XauWriteAuth"
		else
			LIB_XauReadAuth="$LIB_XauReadAuth $LIB_XauWriteAuth $LIB_XauFileName"
		fi
	fi
	;;
esac

if test "$AUTOMAKE" != ""; then
	AM_CONDITIONAL(NEED_WRITEAUTH, test "$ac_cv_func_XauWriteAuth" != "yes")
else
	AC_SUBST(NEED_WRITEAUTH_TRUE)
	AC_SUBST(NEED_WRITEAUTH_FALSE)
	if test "$ac_cv_func_XauWriteAuth" != "yes"; then
		NEED_WRITEAUTH_TRUE=
		NEED_WRITEAUTH_FALSE='#'
	else
		NEED_WRITEAUTH_TRUE='#'
		NEED_WRITEAUTH_FALSE=
	fi
fi
CFLAGS=$save_CFLAGS
LIBS=$save_LIBS
LDFLAGS=$save_LDFLAGS
])

dnl
dnl $KTH: capabilities.m4,v 1.2 1999/09/01 11:02:26 joda Exp $
dnl

dnl
dnl Test SGI capabilities
dnl

AC_DEFUN(KRB_CAPABILITIES,[

AC_CHECK_HEADERS(capability.h sys/capability.h)

AC_CHECK_FUNCS(sgi_getcapabilitybyname cap_set_proc)
])

dnl $KTH: check-getpwnam_r-posix.m4,v 1.2 1999/03/23 16:47:31 joda Exp $
dnl
dnl check for getpwnam_r, and if it's posix or not

AC_DEFUN(AC_CHECK_GETPWNAM_R_POSIX,[
AC_FIND_FUNC_NO_LIBS(getpwnam_r,c_r)
if test "$ac_cv_func_getpwnam_r" = yes; then
	AC_CACHE_CHECK(if getpwnam_r is posix,ac_cv_func_getpwnam_r_posix,
	ac_libs="$LIBS"
	LIBS="$LIBS $LIB_getpwnam_r"
	AC_TRY_RUN([
#include <pwd.h>
int main()
{
	struct passwd pw, *pwd;
	return getpwnam_r("", &pw, NULL, 0, &pwd) < 0;
}
],ac_cv_func_getpwnam_r_posix=yes,ac_cv_func_getpwnam_r_posix=no,:)
LIBS="$ac_libs")
if test "$ac_cv_func_getpwnam_r_posix" = yes; then
	AC_DEFINE(POSIX_GETPWNAM_R, 1, [Define if getpwnam_r has POSIX flavour.])
fi
fi
])
dnl $KTH: grok-type.m4,v 1.4 1999/11/29 11:16:48 joda Exp $
dnl
AC_DEFUN(AC_GROK_TYPE, [
AC_CACHE_VAL(ac_cv_type_$1, 
AC_TRY_COMPILE([
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#ifdef HAVE_BIND_BITYPES_H
#include <bind/bitypes.h>
#endif
#ifdef HAVE_NETINET_IN6_MACHTYPES_H
#include <netinet/in6_machtypes.h>
#endif
],
$i x;
,
eval ac_cv_type_$1=yes,
eval ac_cv_type_$1=no))])

AC_DEFUN(AC_GROK_TYPES, [
for i in $1; do
	AC_MSG_CHECKING(for $i)
	AC_GROK_TYPE($i)
	eval ac_res=\$ac_cv_type_$i
	if test "$ac_res" = yes; then
		type=HAVE_[]upcase($i)
		AC_DEFINE_UNQUOTED($type)
	fi
	AC_MSG_RESULT($ac_res)
done
])

dnl $KTH: crypto.m4,v 1.7 2001/08/29 17:02:48 assar Exp $
dnl
dnl test for crypto libraries:
dnl - libcrypto (from openssl)
dnl - libdes (from krb4)
dnl - own-built libdes

AC_DEFUN([KRB_CRYPTO],[
crypto_lib=unknown
AC_WITH_ALL([openssl])

DIR_des=

AC_MSG_CHECKING([for crypto library])

if test "$crypto_lib" = "unknown" -a "$with_openssl" != "no"; then

  save_CPPFLAGS="$CPPFLAGS"
  save_LIBS="$LIBS"
  INCLUDE_des=
  LIB_des=
  if test "$with_openssl_include" != ""; then
    INCLUDE_des="-I${with_openssl}/include"
  fi
  if test "$with_openssl_lib" != ""; then
    LIB_des="-L${with_openssl}/lib"
  fi
  CPPFLAGS="${INCLUDE_des} ${CPPFLAGS}"
  LIB_des="${LIB_des} -lcrypto"
  LIB_des_a="$LIB_des"
  LIB_des_so="$LIB_des"
  LIB_des_appl="$LIB_des"
  LIBS="${LIBS} ${LIB_des}"
  AC_TRY_LINK([
  #include <openssl/md4.h>
  #include <openssl/md5.h>
  #include <openssl/sha.h>
  #include <des.h>
  #include <openssl/rc4.h>
  ],
  [
    MD4_CTX md4;
    MD5_CTX md5;
    SHA_CTX sha1;

    MD4_Init(&md4);
    MD5_Init(&md5);
    SHA1_Init(&sha1);

    des_cbc_encrypt(0, 0, 0, 0, 0, 0);
    RC4(0, 0, 0, 0);
  ], [
  crypto_lib=libcrypto
  AC_DEFINE([HAVE_OPENSSL], 1, [define to use openssl's libcrypto])
  AC_MSG_RESULT([libcrypto])])
  CPPFLAGS="$save_CPPFLAGS"
  LIBS="$save_LIBS"
fi

if test "$crypto_lib" = "unknown" -a "$with_krb4" != "no"; then

  save_CPPFLAGS="$CPPFLAGS"
  save_LIBS="$LIBS"
  INCLUDE_des="${INCLUDE_krb4}"
  LIB_des=
  if test "$krb4_libdir"; then
    LIB_des="-L${krb4_libdir}"
  fi
  LIB_des="${LIB_des} -ldes"
  CPPFLAGS="${CPPFLAGS} ${INCLUDE_des}"
  LIBS="${LIBS} ${LIB_des}"
  LIB_des_a="$LIB_des"
  LIB_des_so="$LIB_des"
  LIB_des_appl="$LIB_des"
  LIBS="${LIBS} ${LIB_des}"
  AC_TRY_LINK([
  #undef KRB5 /* makes md4.h et al unhappy */
  #define KRB4
  #include <md4.h>
  #include <md5.h>
  #include <sha.h>
  #include <des.h>
  #include <rc4.h>
  ],
  [
    MD4_CTX md4;
    MD5_CTX md5;
    SHA_CTX sha1;

    MD4_Init(&md4);
    MD5_Init(&md5);
    SHA1_Init(&sha1);

    des_cbc_encrypt(0, 0, 0, 0, 0, 0);
    RC4(0, 0, 0, 0);
  ], [crypto_lib=krb4; AC_MSG_RESULT([krb4's libdes])])
  CPPFLAGS="$save_CPPFLAGS"
  LIBS="$save_LIBS"
fi

if test "$crypto_lib" = "unknown"; then

  DIR_des='des'
  LIB_des='$(top_builddir)/lib/des/libdes.la'
  LIB_des_a='$(top_builddir)/lib/des/.libs/libdes.a'
  LIB_des_so='$(top_builddir)/lib/des/.libs/libdes.so'
  LIB_des_appl="-ldes"

  AC_MSG_RESULT([included libdes])

fi

AC_SUBST(DIR_des)
AC_SUBST(INCLUDE_des)
AC_SUBST(LIB_des)
AC_SUBST(LIB_des_a)
AC_SUBST(LIB_des_so)
AC_SUBST(LIB_des_appl)
])

dnl
dnl $KTH: with-all.m4,v 1.1 2001/08/29 17:01:23 assar Exp $
dnl

dnl AC_WITH_ALL(name)

AC_DEFUN([AC_WITH_ALL], [
AC_ARG_WITH($1,
	AC_HELP_STRING([--with-$1=dir],
		[use $1 in dir]))

AC_ARG_WITH($1-lib,
	AC_HELP_STRING([--with-$1-lib=dir],
		[use $1 libraries in dir]),
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-lib])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])

AC_ARG_WITH($1-include,
	AC_HELP_STRING([--with-$1-include=dir],
		[use $1 headers in dir]),
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-include])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])

case "$with_$1" in
yes)	;;
no)	;;
"")	;;
*)	if test "$with_$1_include" = ""; then
		with_$1_include="$with_$1/include"
	fi
	if test "$with_$1_lib" = ""; then
		with_$1_lib="$with_$1/lib$abilibdirext"
	fi
	;;
esac
])
dnl $KTH: krb-readline.m4,v 1.2 2000/11/15 00:47:08 assar Exp $
dnl
dnl Tests for readline functions
dnl

dnl el_init

AC_DEFUN(KRB_READLINE,[
AC_FIND_FUNC_NO_LIBS(el_init, edit, [], [], [$LIB_tgetent])
if test "$ac_cv_func_el_init" = yes ; then
	AC_CACHE_CHECK(for four argument el_init, ac_cv_func_el_init_four,[
		AC_TRY_COMPILE([#include <stdio.h>
			#include <histedit.h>],
			[el_init("", NULL, NULL, NULL);],
			ac_cv_func_el_init_four=yes,
			ac_cv_func_el_init_four=no)])
	if test "$ac_cv_func_el_init_four" = yes; then
		AC_DEFINE(HAVE_FOUR_VALUED_EL_INIT, 1, [Define if el_init takes four arguments.])
	fi
fi

dnl readline

ac_foo=no
if test "$with_readline" = yes; then
	:
elif test "$ac_cv_func_readline" = yes; then
	:
elif test "$ac_cv_func_el_init" = yes; then
	ac_foo=yes
	LIB_readline="\$(top_builddir)/lib/editline/libel_compat.la $LIB_el_init"
else
	LIB_readline='$(top_builddir)/lib/editline/libeditline.la'
fi
AM_CONDITIONAL(el_compat, test "$ac_foo" = yes)
if test "$readline_libdir"; then
	LIB_readline="-rpath $readline_libdir $LIB_readline"
fi
LIB_readline="$LIB_readline \$(LIB_tgetent)"
AC_DEFINE(HAVE_READLINE, 1, 
	[Define if you have a readline compatible library.])dnl

])
dnl $KTH: check-compile-et.m4,v 1.6 2001/09/02 17:08:48 assar Exp $
dnl
dnl CHECK_COMPILE_ET
AC_DEFUN([CHECK_COMPILE_ET], [

AC_CHECK_PROG(COMPILE_ET, compile_et, [compile_et])

krb_cv_compile_et="no"
if test "${COMPILE_ET}" = "compile_et"; then

dnl We have compile_et.  Now let's see if it supports `prefix' and `index'.
AC_MSG_CHECKING(whether compile_et has the features we need)
cat > conftest_et.et <<'EOF'
error_table conf
prefix CONFTEST
index 1
error_code CODE1, "CODE1"
index 128
error_code CODE2, "CODE2"
end
EOF
if ${COMPILE_ET} conftest_et.et >/dev/null 2>&1; then
  dnl XXX Some systems have <et/com_err.h>.
  save_CPPFLAGS="${save_CPPFLAGS}"
  if test -d "/usr/include/et"; then
    CPPFLAGS="-I/usr/include/et ${CPPFLAGS}"
  fi
  dnl Check that the `prefix' and `index' directives were honored.
  AC_TRY_RUN([
#include <com_err.h>
#include <string.h>
#include "conftest_et.h"
int main(){return (CONFTEST_CODE2 - CONFTEST_CODE1) != 127;}
  ], [krb_cv_compile_et="yes"],[CPPFLAGS="${save_CPPFLAGS}"])
fi
AC_MSG_RESULT(${krb_cv_compile_et})
rm -fr conftest*
fi

if test "${krb_cv_compile_et}" = "yes"; then
  dnl Since compile_et seems to work, let's check libcom_err
  krb_cv_save_LIBS="${LIBS}"
  LIBS="${LIBS} -lcom_err"
  AC_MSG_CHECKING(for com_err)
  AC_TRY_LINK([#include <com_err.h>],[
    const char *p;
    p = error_message(0);
  ],[krb_cv_com_err="yes"],[krb_cv_com_err="no"; CPPFLAGS="${save_CPPFLAGS}"])
  AC_MSG_RESULT(${krb_cv_com_err})
  LIBS="${krb_cv_save_LIBS}"
else
  dnl Since compile_et doesn't work, forget about libcom_err
  krb_cv_com_err="no"
fi

dnl Only use the system's com_err if we found compile_et, libcom_err, and
dnl com_err.h.
if test "${krb_cv_com_err}" = "yes"; then
    DIR_com_err=""
    LIB_com_err="-lcom_err"
    LIB_com_err_a=""
    LIB_com_err_so=""
    AC_MSG_NOTICE(Using the already-installed com_err)
else
    COMPILE_ET="\$(top_builddir)/lib/com_err/compile_et"
    DIR_com_err="com_err"
    LIB_com_err="\$(top_builddir)/lib/com_err/libcom_err.la"
    LIB_com_err_a="\$(top_builddir)/lib/com_err/.libs/libcom_err.a"
    LIB_com_err_so="\$(top_builddir)/lib/com_err/.libs/libcom_err.so"
    AC_MSG_NOTICE(Using our own com_err)
fi
AC_SUBST(DIR_com_err)
AC_SUBST(LIB_com_err)
AC_SUBST(LIB_com_err_a)
AC_SUBST(LIB_com_err_so)

])

dnl $KTH: auth-modules.m4,v 1.2 2001/09/01 11:56:05 assar Exp $
dnl
dnl Figure what authentication modules should be built

AC_DEFUN(AC_AUTH_MODULES,[
AC_MSG_CHECKING(which authentication modules should be built)

LIB_AUTH_SUBDIRS=

if test "$ac_cv_header_siad_h" = yes; then
	LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS sia"
fi

if test "$ac_cv_header_security_pam_modules_h" = yes -a "$enable_shared" = yes; then
	LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS pam"
fi

case "${host}" in
*-*-irix[[56]]*) LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS afskauthlib" ;;
esac

AC_MSG_RESULT($LIB_AUTH_SUBDIRS)

AC_SUBST(LIB_AUTH_SUBDIRS)dnl
])

