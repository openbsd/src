dnl Macros for auto-configure script.
dnl by T.E.Dickey <dickey@clark.net>
dnl and Jim Spath <jspath@mail.bcpl.lib.md.us>
dnl and Philippe De Muyter <phdm@macqel.be>
dnl
dnl Created: 1997/1/28
dnl Updated: 1999/5/8
dnl
dnl The autoconf used in Lynx development is GNU autoconf, patched
dnl by Tom Dickey.  See your local GNU archives, and this URL:
dnl http://www.clark.net/pub/dickey/autoconf/autoconf.html
dnl
dnl ---------------------------------------------------------------------------
dnl ---------------------------------------------------------------------------
dnl
AC_DEFUN(AM_GNU_GETTEXT,
  [AC_REQUIRE([AC_PROG_MAKE_SET])dnl
   AC_REQUIRE([AC_PROG_CC])dnl
   AC_REQUIRE([AC_PROG_RANLIB])dnl
   AC_REQUIRE([AC_ISC_POSIX])dnl
   AC_REQUIRE([AC_HEADER_STDC])dnl
   AC_REQUIRE([AC_C_CONST])dnl
   AC_REQUIRE([AC_C_INLINE])dnl
   AC_REQUIRE([AC_TYPE_OFF_T])dnl
   AC_REQUIRE([AC_TYPE_SIZE_T])dnl
   AC_REQUIRE([AC_FUNC_ALLOCA])dnl
   AC_REQUIRE([AC_FUNC_MMAP])dnl

   AC_CHECK_HEADERS([argz.h limits.h locale.h nl_types.h malloc.h string.h \
unistd.h values.h sys/param.h])
   AC_CHECK_FUNCS([getcwd munmap putenv setenv setlocale strchr strcasecmp \
__argz_count __argz_stringify __argz_next])

   if test "${ac_cv_func_stpcpy+set}" != "set"; then
     AC_CHECK_FUNCS(stpcpy)
   fi
   if test "${ac_cv_func_stpcpy}" = "yes"; then
     AC_DEFINE(HAVE_STPCPY)
   fi

   AM_LC_MESSAGES
   AM_WITH_NLS

   if test "x$CATOBJEXT" != "x"; then
     if test "x$ALL_LINGUAS" = "x"; then
       LINGUAS=
     else
       AC_MSG_CHECKING(for catalogs to be installed)
       NEW_LINGUAS=
       for lang in ${LINGUAS=$ALL_LINGUAS}; do
         case "$ALL_LINGUAS" in
          *$lang*) NEW_LINGUAS="$NEW_LINGUAS $lang" ;;
         esac
       done
       LINGUAS=$NEW_LINGUAS
       AC_MSG_RESULT($LINGUAS)
     fi

     dnl Construct list of names of catalog files to be constructed.
     if test -n "$LINGUAS"; then
       for lang in $LINGUAS; do CATALOGS="$CATALOGS $lang$CATOBJEXT"; done
     fi
   fi

   dnl The reference to <locale.h> in the installed <libintl.h> file
   dnl must be resolved because we cannot expect the users of this
   dnl to define HAVE_LOCALE_H.
   if test $ac_cv_header_locale_h = yes; then
     INCLUDE_LOCALE_H="#include <locale.h>"
   else
     INCLUDE_LOCALE_H="\
/* The system does not provide the header <locale.h>.  Take care of it yourself. */"
   fi
   AC_SUBST(INCLUDE_LOCALE_H)

   dnl Determine which catalog format we have (if any is needed)
   dnl For now we know about two different formats:
   dnl   Linux libc-5 and the normal X/Open format
   if test "$USE_NLS" = "yes"; then
     test -d intl || mkdir intl
     if test "$CATOBJEXT" = ".cat"; then
       AC_CHECK_HEADER(linux/version.h, msgformat=linux, msgformat=xopen)

       dnl Transform the SED scripts while copying because some dumb SEDs
       dnl cannot handle comments.
       sed -e '/^#/d' $srcdir/intl/$msgformat-msg.sed > intl/po2msg.sed
     fi
     dnl po2tbl.sed is always needed.
     if test -f $srcdir/intl/po2tbl.sed.in ; then
       rm -f intl/po2tbl.sed
       sed -e '/^#.*[^\\]$/d' -e '/^#$/d' \
         $srcdir/intl/po2tbl.sed.in > intl/po2tbl.sed
     fi
   fi

   dnl In the intl/makefile.in we have a special dependency which only
   dnl makes sense for gettext.  We comment this out for non-gettext
   dnl packages.
   if test "$PACKAGE" = "gettext"; then
     GT_NO="#NO#"
     GT_YES=
   else
     GT_NO=
     GT_YES="#YES#"
   fi
   AC_SUBST(GT_NO)
   AC_SUBST(GT_YES)

   dnl If the AC_CONFIG_AUX_DIR macro for autoconf is used we possibly
   dnl find the mkinstalldirs script in another subdir but ($top_srcdir).
   dnl Try to locate it.
   dnl changed mkinstalldirs to mkdirs.sh for Lynx /je spath 1998-Aug-21
   MKINSTALLDIRS=
   if test -n "$ac_aux_dir"; then
     MKINSTALLDIRS="$ac_aux_dir/mkdirs.sh"
   fi
   if test -z "$MKINSTALLDIRS"; then
     MKINSTALLDIRS="\$(top_srcdir)/mkdirs.sh"
   fi
   AC_SUBST(MKINSTALLDIRS)

   dnl *** For now the libtool support in intl/makefile is not for real.
   l=
   AC_SUBST(l)

   dnl Generate list of files to be processed by xgettext which will
   dnl be included in po/makefile.
   if test "x$srcdir" != "x."; then
     if test "x`echo $srcdir | sed 's@/.*@@'`" = "x"; then
       posrcprefix="$srcdir/"
     else
       posrcprefix="../$srcdir/"
     fi
   else
     posrcprefix="../"
   fi
   if test -f $srcdir/po/POTFILES.in ; then
   if test "$USE_NLS" = "yes"; then
     test -d po || mkdir po
     rm -f po/POTFILES
     sed -e "/^#/d" -e "/^\$/d" -e "s,.*,	$posrcprefix& \\\\," -e "\$s/\(.*\) \\\\/\1/" \
	  < $srcdir/po/POTFILES.in > po/POTFILES
   fi
   fi
])dnl
dnl ---------------------------------------------------------------------------
dnl
dnl Check whether LC_MESSAGES is available in <locale.h>.
dnl Ulrich Drepper <drepper@cygnus.com>, 1995.
dnl
dnl This file can be copied and used freely without restrictions.  It can
dnl be used in projects which are not available under the GNU Public License
dnl but which still want to provide support for the GNU gettext functionality.
dnl Please note that the actual code is *not* freely available.
dnl
dnl serial 1
dnl
AC_DEFUN(AM_LC_MESSAGES,
  [if test $ac_cv_header_locale_h = yes; then
    AC_CACHE_CHECK([for LC_MESSAGES], am_cv_val_LC_MESSAGES,
      [AC_TRY_LINK([#include <locale.h>], [return LC_MESSAGES],
       am_cv_val_LC_MESSAGES=yes, am_cv_val_LC_MESSAGES=no)])
    if test $am_cv_val_LC_MESSAGES = yes; then
      AC_DEFINE(HAVE_LC_MESSAGES)
    fi
  fi])

dnl ---------------------------------------------------------------------------
dnl Search path for a program which passes the given test.
dnl Ulrich Drepper <drepper@cygnus.com>, 1996.
dnl
dnl This file can be copied and used freely without restrictions.  It can
dnl be used in projects which are not available under the GNU Public License
dnl but which still want to provide support for the GNU gettext functionality.
dnl Please note that the actual code is *not* freely available.
dnl
dnl serial 1
dnl
dnl
dnl AM_PATH_PROG_WITH_TEST(VARIABLE, PROG-TO-CHECK-FOR,
dnl   TEST-PERFORMED-ON-FOUND_PROGRAM [, VALUE-IF-NOT-FOUND [, PATH]])
AC_DEFUN(AM_PATH_PROG_WITH_TEST,
[# Extract the first word of "$2", so it can be a program name with args.
set dummy $2; ac_word=[$]2
AC_MSG_CHECKING([for $ac_word])
AC_CACHE_VAL(ac_cv_path_$1,
[case "[$]$1" in
  /*)
  ac_cv_path_$1="[$]$1" # Let the user override the test with a path.
  ;;
  *)
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:"
  for ac_dir in ifelse([$5], , $PATH, [$5]); do
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/$ac_word; then
      if [$3]; then
	ac_cv_path_$1="$ac_dir/$ac_word"
	break
      fi
    fi
  done
  IFS="$ac_save_ifs"
dnl If no 4th arg is given, leave the cache variable unset,
dnl so AC_PATH_PROGS will keep looking.
ifelse([$4], , , [  test -z "[$]ac_cv_path_$1" && ac_cv_path_$1="$4"
])dnl
  ;;
esac])dnl
$1="$ac_cv_path_$1"
if test -n "[$]$1"; then
  AC_MSG_RESULT([$]$1)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST($1)dnl
])

dnl ---------------------------------------------------------------------------
dnl gettext macros from hello-1.3.16
dnl provided courtesy if Karl Eichwalder <ke@suse.de>
dnl
dnl
dnl Macro to add for using GNU gettext.
dnl Ulrich Drepper <drepper@cygnus.com>, 1995.
dnl
dnl This file can be copied and used freely without restrictions.  It can
dnl be used in projects which are not available under the GNU Public License
dnl but which still want to provide support for the GNU gettext functionality.
dnl Please note that the actual code is *not* freely available.
dnl
dnl serial 3
dnl
AC_DEFUN(AM_WITH_NLS,
  [AC_MSG_CHECKING([whether NLS is requested])
    dnl Default is enabled NLS
    AC_ARG_ENABLE(nls,
      [  --enable-nls            use Native Language Support],
      USE_NLS=$enableval, USE_NLS=no)
    AC_MSG_RESULT($USE_NLS)
    AC_SUBST(USE_NLS)

    USE_INCLUDED_LIBINTL=no

    dnl If we use NLS figure out what method
    if test "$USE_NLS" = "yes"; then
      AC_DEFINE(ENABLE_NLS)
      AC_MSG_CHECKING([whether included gettext is requested])
      AC_ARG_WITH(included-gettext,
        [  --with-included-gettext use the GNU gettext library included here],
        nls_cv_force_use_gnu_gettext=$withval,
        nls_cv_force_use_gnu_gettext=yes)
      AC_MSG_RESULT($nls_cv_force_use_gnu_gettext)

      nls_cv_use_gnu_gettext="$nls_cv_force_use_gnu_gettext"
      if test "$nls_cv_force_use_gnu_gettext" != "yes"; then
        dnl User does not insist on using GNU NLS library.  Figure out what
        dnl to use.  If gettext or catgets are available (in this order) we
        dnl use this.  Else we have to fall back to GNU NLS library.
	dnl catgets is only used if permitted by option --with-catgets.
	nls_cv_header_intl=
	nls_cv_header_libgt=
	CATOBJEXT=NONE

	AC_CHECK_HEADER(libintl.h,
	  [AC_CACHE_CHECK([for gettext in libc], gt_cv_func_gettext_libc,
	    [AC_TRY_LINK([#include <libintl.h>], [return (int) gettext ("")],
	       gt_cv_func_gettext_libc=yes, gt_cv_func_gettext_libc=no)])

	   if test "$gt_cv_func_gettext_libc" != "yes"; then
	     AC_CHECK_LIB(intl, bindtextdomain,
	       [AC_CACHE_CHECK([for gettext in libintl],
		 gt_cv_func_gettext_libintl,
		 [AC_TRY_LINK([], [return (int) gettext ("")],
		 gt_cv_func_gettext_libintl=yes,
		 gt_cv_func_gettext_libintl=no)])])
	   fi

	   if test "$gt_cv_func_gettext_libc" = "yes" \
	      || test "$gt_cv_func_gettext_libintl" = "yes"; then
	      AC_DEFINE(HAVE_GETTEXT)
	      AM_PATH_PROG_WITH_TEST(MSGFMT, msgfmt,
		[test -z "`$ac_dir/$ac_word -h 2>&1 | grep 'dv '`"], no)dnl
	      if test "$MSGFMT" != "no"; then
		AC_CHECK_FUNCS(dcgettext)
		AC_PATH_PROG(GMSGFMT, gmsgfmt, $MSGFMT)
		AM_PATH_PROG_WITH_TEST(XGETTEXT, xgettext,
		  [test -z "`$ac_dir/$ac_word -h 2>&1 | grep '(HELP)'`"], :)
		AC_TRY_LINK(, [extern int _nl_msg_cat_cntr;
			       return _nl_msg_cat_cntr],
		  [CATOBJEXT=.gmo
		   DATADIRNAME=share],
		  [CATOBJEXT=.mo
		   DATADIRNAME=lib])
		INSTOBJEXT=.mo
	      fi
	    fi
	])

        if test "$CATOBJEXT" = "NONE"; then
	  AC_MSG_CHECKING([whether catgets can be used])
	  AC_ARG_WITH(catgets,
	    [  --with-catgets          use catgets functions if available],
	    nls_cv_use_catgets=$withval, nls_cv_use_catgets=no)
	  AC_MSG_RESULT($nls_cv_use_catgets)

	  if test "$nls_cv_use_catgets" = "yes"; then
	    dnl No gettext in C library.  Try catgets next.
	    AC_CHECK_LIB(i, main)
	    AC_CHECK_FUNC(catgets,
	      [AC_DEFINE(HAVE_CATGETS)
	       INTLOBJS="\$(CATOBJS)"
	       AC_PATH_PROG(GENCAT, gencat, no)dnl
	       if test "$GENCAT" != "no"; then
		 AC_PATH_PROG(GMSGFMT, gmsgfmt, no)
		 if test "$GMSGFMT" = "no"; then
		   AM_PATH_PROG_WITH_TEST(GMSGFMT, msgfmt,
		    [test -z "`$ac_dir/$ac_word -h 2>&1 | grep 'dv '`"], no)
		 fi
		 AM_PATH_PROG_WITH_TEST(XGETTEXT, xgettext,
		   [test -z "`$ac_dir/$ac_word -h 2>&1 | grep '(HELP)'`"], :)
		 USE_INCLUDED_LIBINTL=yes
		 CATOBJEXT=.cat
		 INSTOBJEXT=.cat
		 DATADIRNAME=lib
		 INTLDEPS='$(top_builddir)/intl/libintl.a'
		 INTLLIBS=$INTLDEPS
		 LIBS=`echo $LIBS | sed -e 's/-lintl//'`
		 nls_cv_header_intl=intl/libintl.h
		 nls_cv_header_libgt=intl/libgettext.h
	       fi])
	  fi
        fi

        if test "$CATOBJEXT" = "NONE"; then
	  dnl Neither gettext nor catgets in included in the C library.
	  dnl Fall back on GNU gettext library.
	  nls_cv_use_gnu_gettext=yes
        fi
      fi

      if test "$nls_cv_use_gnu_gettext" = "yes"; then
        dnl Mark actions used to generate GNU NLS library.
        INTLOBJS="\$(GETTOBJS)"
        AM_PATH_PROG_WITH_TEST(MSGFMT, msgfmt,
	  [test -z "`$ac_dir/$ac_word -h 2>&1 | grep 'dv '`"], msgfmt)
        AC_PATH_PROG(GMSGFMT, gmsgfmt, $MSGFMT)
        AM_PATH_PROG_WITH_TEST(XGETTEXT, xgettext,
	  [test -z "`$ac_dir/$ac_word -h 2>&1 | grep '(HELP)'`"], :)
        AC_SUBST(MSGFMT)
	USE_INCLUDED_LIBINTL=yes
        CATOBJEXT=.gmo
        INSTOBJEXT=.mo
        DATADIRNAME=share
	INTLDEPS='$(top_builddir)/intl/libintl.a'
	INTLLIBS=$INTLDEPS
	LIBS=`echo $LIBS | sed -e 's/-lintl//'`
        nls_cv_header_intl=intl/libintl.h
        nls_cv_header_libgt=intl/libgettext.h
      fi

      dnl Test whether we really found GNU xgettext.
      if test "$XGETTEXT" != ":"; then
	dnl If it is no GNU xgettext we define it as : so that the
	dnl makefiles still can work.
	if $XGETTEXT --omit-header /dev/null 2> /dev/null; then
	  : ;
	else
	  AC_MSG_RESULT(
	    [found xgettext programs is not GNU xgettext; ignore it])
	  XGETTEXT=":"
	fi
      fi

      # We need to process the po/ directory.
      POSUB=po
    else
      DATADIRNAME=share
      nls_cv_header_intl=intl/libintl.h
      nls_cv_header_libgt=intl/libgettext.h
    fi

    # If this is used in GNU gettext we have to set USE_NLS to `yes'
    # because some of the sources are only built for this goal.
    if test "$PACKAGE" = gettext; then
      USE_NLS=yes
      USE_INCLUDED_LIBINTL=yes
    fi

    dnl These rules are solely for the distribution goal.  While doing this
    dnl we only have to keep exactly one list of the available catalogs
    dnl in configure.in.
    for lang in $ALL_LINGUAS; do
      GMOFILES="$GMOFILES $lang.gmo"
      POFILES="$POFILES $lang.po"
    done

    dnl Make all variables we use known to autoconf.
    AC_SUBST(USE_INCLUDED_LIBINTL)
    AC_SUBST(CATALOGS)
    AC_SUBST(CATOBJEXT)
    AC_SUBST(DATADIRNAME)
    AC_SUBST(GMOFILES)
    AC_SUBST(INSTOBJEXT)
    AC_SUBST(INTLDEPS)
    AC_SUBST(INTLLIBS)
    AC_SUBST(INTLOBJS)
    AC_SUBST(POFILES)
    AC_SUBST(POSUB)
  ])
dnl ---------------------------------------------------------------------------
dnl Add an include-directory to $CPPFLAGS.  Don't add /usr/include, since it's
dnl redundant.  We don't normally need to add -I/usr/local/include for gcc,
dnl but old versions (and some misinstalled ones) need that.
AC_DEFUN([CF_ADD_INCDIR],
[
for cf_add_incdir in $1
do
	while true
	do
		case $cf_add_incdir in
		/usr/include) # (vi
			;;
		*) # (vi
			CPPFLAGS="$CPPFLAGS -I$cf_add_incdir"
			;;
		esac
		cf_top_incdir=`echo $cf_add_incdir | sed -e 's:/include/.*$:/include:'`
		test "$cf_top_incdir" = "$cf_add_incdir" && break
		cf_add_incdir="$cf_top_incdir"
	done
done
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for existence of alternate-character-set support in curses, so we
dnl can decide to use it for box characters.
dnl
AC_DEFUN([CF_ALT_CHAR_SET],
[
AC_MSG_CHECKING([if curses supports alternate-character set])
AC_CACHE_VAL(cf_cv_alt_char_set,[
for mapname in acs_map _acs_map
do
	AC_TRY_LINK([
#include <${cf_cv_ncurses_header-curses.h}>
	],[chtype x = $mapname['l']; $mapname['m'] = 0],
	[cf_cv_alt_char_set=$mapname
	 break],
	[cf_cv_alt_char_set=no])
done
	])
AC_MSG_RESULT($cf_cv_alt_char_set)
test $cf_cv_alt_char_set != no && AC_DEFINE_UNQUOTED(ALT_CHAR_SET,$cf_cv_alt_char_set)
])dnl
dnl ---------------------------------------------------------------------------
dnl This is adapted from the macros 'fp_PROG_CC_STDC' and 'fp_C_PROTOTYPES'
dnl in the sharutils 4.2 distribution.
AC_DEFUN([CF_ANSI_CC_CHECK],
[
AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(cf_cv_ansi_cc,[
cf_cv_ansi_cc=no
cf_save_CFLAGS="$CFLAGS"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc
# UnixWare 1.2		(cannot use -Xc, since ANSI/POSIX clashes)
for cf_arg in "-DCC_HAS_PROTOS" \
	"" \
	-qlanglvl=ansi \
	-std1 \
	"-Aa -D_HPUX_SOURCE +e" \
	"-Aa -D_HPUX_SOURCE" \
	-Xc
do
	CFLAGS="$cf_save_CFLAGS $cf_arg"
	AC_TRY_COMPILE(
[
#ifndef CC_HAS_PROTOS
#if !defined(__STDC__) || (__STDC__ != 1)
choke me
#endif
#endif
],[
	int test (int i, double x);
	struct s1 {int (*f) (int a);};
	struct s2 {int (*f) (double a);};],
	[cf_cv_ansi_cc="$cf_arg"; break])
done
CFLAGS="$cf_save_CFLAGS"
])
AC_MSG_RESULT($cf_cv_ansi_cc)

if test "$cf_cv_ansi_cc" != "no"; then
if test ".$cf_cv_ansi_cc" != ".-DCC_HAS_PROTOS"; then
	CFLAGS="$CFLAGS $cf_cv_ansi_cc"
else
	AC_DEFINE(CC_HAS_PROTOS)
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Allow user to disable a normally-on option.
AC_DEFUN([CF_ARG_DISABLE],
[CF_ARG_OPTION($1,[$2],[$3],[$4],yes)])dnl
dnl ---------------------------------------------------------------------------
dnl Allow user to enable a normally-off option.
AC_DEFUN([CF_ARG_ENABLE],
[CF_ARG_OPTION($1,[$2],[$3],[$4],no)])dnl
dnl ---------------------------------------------------------------------------
dnl Restricted form of AC_ARG_ENABLE that ensures user doesn't give bogus
dnl values.
dnl
dnl Parameters:
dnl $1 = option name
dnl $2 = help-string
dnl $3 = action to perform if option is not default
dnl $4 = action if perform if option is default
dnl $5 = default option value (either 'yes' or 'no')
AC_DEFUN([CF_ARG_OPTION],
[AC_ARG_ENABLE($1,[$2],[test "$enableval" != ifelse($5,no,yes,no) && enableval=ifelse($5,no,no,yes)
  if test "$enableval" != "$5" ; then
ifelse($3,,[    :]dnl
,[    $3]) ifelse($4,,,[
  else
    $4])
  fi],[enableval=$5 ifelse($4,,,[
  $4
])dnl
  ])])dnl
dnl ---------------------------------------------------------------------------
dnl Check if curses.h defines TRUE/FALSE (it does under SVr4).
AC_DEFUN([CF_BOOL_DEFS],
[
AC_MSG_CHECKING(if TRUE/FALSE are defined)
AC_CACHE_VAL(cf_cv_bool_defs,[
AC_TRY_COMPILE([
#include <${cf_cv_ncurses_header-curses.h}>
#include <stdio.h>],[int x = TRUE, y = FALSE],
	[cf_cv_bool_defs=yes],
	[cf_cv_bool_defs=no])])
AC_MSG_RESULT($cf_cv_bool_defs)
if test "$cf_cv_bool_defs" = no ; then
	AC_DEFINE(TRUE,(1))
	AC_DEFINE(FALSE,(0))
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we're accidentally using a cache from a different machine.
dnl Derive the system name, as a check for reusing the autoconf cache.
dnl
dnl If we've packaged config.guess and config.sub, run that (since it does a
dnl better job than uname).
AC_DEFUN([CF_CHECK_CACHE],
[
if test -f $srcdir/config.guess ; then
	AC_CANONICAL_HOST
	system_name="$host_os"
else
	system_name="`(uname -s -r) 2>/dev/null`"
	if test -z "$system_name" ; then
		system_name="`(hostname) 2>/dev/null`"
	fi
fi
test -n "$system_name" && AC_DEFINE_UNQUOTED(SYSTEM_NAME,"$system_name")
AC_CACHE_VAL(cf_cv_system_name,[cf_cv_system_name="$system_name"])

test -z "$system_name" && system_name="$cf_cv_system_name"
test -n "$cf_cv_system_name" && AC_MSG_RESULT("Configuring for $cf_cv_system_name")

if test ".$system_name" != ".$cf_cv_system_name" ; then
	AC_MSG_RESULT(Cached system name ($system_name) does not agree with actual ($cf_cv_system_name))
	AC_ERROR("Please remove config.cache and try again.")
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for data that is usually declared in <stdio.h> or <errno.h>, e.g.,
dnl the 'errno' variable.  Define a DECL_xxx symbol if we must declare it
dnl ourselves.
dnl
dnl (I would use AC_CACHE_CHECK here, but it will not work when called in a
dnl loop from CF_SYS_ERRLIST).
dnl
dnl $1 = the name to check
AC_DEFUN([CF_CHECK_ERRNO],
[
AC_MSG_CHECKING(if external $1 is declared)
AC_CACHE_VAL(cf_cv_dcl_$1,[
    AC_TRY_COMPILE([
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <errno.h> ],
    [long x = (long) $1],
    [eval 'cf_cv_dcl_'$1'=yes'],
    [eval 'cf_cv_dcl_'$1'=no]')
])

eval 'cf_result=$cf_cv_dcl_'$1
AC_MSG_RESULT($cf_result)

if test "$cf_result" = no ; then
    eval 'cf_result=DECL_'$1
    CF_UPPER(cf_result,$cf_result)
    AC_DEFINE_UNQUOTED($cf_result)
fi

# It's possible (for near-UNIX clones) that the data doesn't exist
CF_CHECK_EXTERN_DATA($1,int)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for existence of external data in the current set of libraries.  If
dnl we can modify it, it's real enough.
dnl $1 = the name to check
dnl $2 = its type
AC_DEFUN([CF_CHECK_EXTERN_DATA],
[
AC_MSG_CHECKING(if external $1 exists)
AC_CACHE_VAL(cf_cv_have_$1,[
    AC_TRY_LINK([
#undef $1
extern $2 $1;
],
    [$1 = 2],
    [eval 'cf_cv_have_'$1'=yes'],
    [eval 'cf_cv_have_'$1'=no'])])

eval 'cf_result=$cf_cv_have_'$1
AC_MSG_RESULT($cf_result)

if test "$cf_result" = yes ; then
    eval 'cf_result=HAVE_'$1
    CF_UPPER(cf_result,$cf_result)
    AC_DEFINE_UNQUOTED($cf_result)
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl Check if a function is declared by including a set of include files.
dnl Invoke the corresponding actions according to whether it is found or not.
dnl
dnl Gcc (unlike other compilers) will only warn about the miscast assignment
dnl in the first test, but most compilers will oblige with an error in the
dnl second test.
dnl
dnl CF_CHECK_FUNCDECL(INCLUDES, FUNCTION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN([CF_CHECK_FUNCDECL],
[
AC_MSG_CHECKING([for $2 declaration])
AC_CACHE_VAL(ac_cv_func_decl_$2,
[AC_TRY_COMPILE([$1],
[#ifndef ${ac_func}
extern	int	${ac_func}();
#endif],[
AC_TRY_COMPILE([$1],
[#ifndef ${ac_func}
int	(*p)() = ${ac_func};
#endif],[
eval "ac_cv_func_decl_$2=yes"],[
eval "ac_cv_func_decl_$2=no"])],[
eval "ac_cv_func_decl_$2=yes"])])
if eval "test \"`echo '$ac_cv_func_'decl_$2`\" = yes"; then
  AC_MSG_RESULT(yes)
  ifelse([$3], , :, [$3])
else
  AC_MSG_RESULT(no)
ifelse([$4], , , [$4
])dnl
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if functions are declared by including a set of include files.
dnl and define DECL_XXX if not.
dnl
dnl CF_CHECK_FUNCDECLS(INCLUDES, FUNCTION... [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN([CF_CHECK_FUNCDECLS],
[for ac_func in $2
do
CF_CHECK_FUNCDECL([$1], $ac_func,
[$3],
[
  CF_UPPER(ac_tr_func,DECL_$ac_func)
  AC_DEFINE_UNQUOTED($ac_tr_func) $4])dnl
done
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if curses supports color.  (Note that while SVr3 curses supports
dnl color, it does this differently from SVr4 curses; more work would be needed
dnl to accommodate SVr3).
dnl
AC_DEFUN([CF_COLOR_CURSES],
[
AC_MSG_CHECKING(if curses supports color attributes)
AC_CACHE_VAL(cf_cv_color_curses,[
	AC_TRY_LINK([
#include <${cf_cv_ncurses_header-curses.h}>
],
	[chtype x = COLOR_BLUE;
	 has_colors();
	 start_color();
#ifndef NCURSES_BROKEN
	 wbkgd(curscr, getbkgd(stdscr)); /* X/Open XPG4 aka SVr4 Curses */
#endif
	],
	[cf_cv_color_curses=yes],
	[cf_cv_color_curses=no])
	])
AC_MSG_RESULT($cf_cv_color_curses)
if test $cf_cv_color_curses = yes ; then
	AC_DEFINE(COLOR_CURSES)
	test ".$cf_cv_ncurses_broken" != .yes && AC_DEFINE(HAVE_GETBKGD)
fi
])
dnl ---------------------------------------------------------------------------
dnl Look for the curses libraries.  Older curses implementations may require
dnl termcap/termlib to be linked as well.
AC_DEFUN([CF_CURSES_LIBS],[
AC_CHECK_FUNC(initscr,,[
case $host_os in #(vi
freebsd*) #(vi
	AC_CHECK_LIB(mytinfo,tgoto,[LIBS="-lmytinfo $LIBS"])
	;;
hpux10.*)
	AC_CHECK_LIB(cur_colr,initscr,[
		LIBS="-lcur_colr $LIBS"
		CFLAGS="-I/usr/include/curses_colr $CFLAGS"
		ac_cv_func_initscr=yes
		],[
	AC_CHECK_LIB(Hcurses,initscr,[
		# HP's header uses __HP_CURSES, but user claims _HP_CURSES.
		LIBS="-lHcurses $LIBS"
		CFLAGS="-D__HP_CURSES -D_HP_CURSES $CFLAGS"
		ac_cv_func_initscr=yes
		])])
	;;
linux*) # Suse Linux does not follow /usr/lib convention
	LIBS="$LIBS -L/lib"
	;;
esac

if test ".$With5lib" != ".no" ; then
if test -d /usr/5lib ; then
	# SunOS 3.x or 4.x
	CPPFLAGS="$CPPFLAGS -I/usr/5include"
	LIBS="$LIBS -L/usr/5lib"
fi
fi

if test ".$ac_cv_func_initscr" != .yes ; then
	cf_save_LIBS="$LIBS"
	cf_term_lib=""
	cf_curs_lib=""

	# Check for library containing tgoto.  Do this before curses library
	# because it may be needed to link the test-case for initscr.
	AC_CHECK_FUNC(tgoto,[cf_term_lib=predefined],[
		for cf_term_lib in termcap termlib unknown
		do
			AC_CHECK_LIB($cf_term_lib,tgoto,[break])
		done
	])

	# Check for library containing initscr
	test "$cf_term_lib" != predefined && test "$cf_term_lib" != unknown && LIBS="-l$cf_term_lib $cf_save_LIBS"
	for cf_curs_lib in cursesX curses ncurses xcurses jcurses unknown
	do
		AC_CHECK_LIB($cf_curs_lib,initscr,[break])
	done
	test $cf_curs_lib = unknown && AC_ERROR(no curses library found)

	LIBS="-l$cf_curs_lib $cf_save_LIBS"
	if test "$cf_term_lib" = unknown ; then
		AC_MSG_CHECKING(if we can link with $cf_curs_lib library)
		AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
			[initscr()],
			[cf_result=yes],
			[cf_result=no])
		AC_MSG_RESULT($cf_result)
		test $cf_result = no && AC_ERROR(Cannot link curses library)
	elif test "$cf_term_lib" != predefined ; then
		AC_MSG_CHECKING(if we need both $cf_curs_lib and $cf_term_lib libraries)
		AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
			[initscr(); tgoto((char *)0, 0, 0);],
			[cf_result=no],
			[
			LIBS="-l$cf_curs_lib -l$cf_term_lib $cf_save_LIBS"
			AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
				[initscr()],
				[cf_result=yes],
				[cf_result=error])
			])
		AC_MSG_RESULT($cf_result)
	fi
fi

])])
dnl ---------------------------------------------------------------------------
dnl Solaris 2.x curses provides a "performance" tradeoff according to whether
dnl CURS_PERFORMANCE is defined.  If defined, the implementation defines macros
dnl that access the WINDOW structure.  Otherwise, function calls are used.
AC_DEFUN([CF_CURS_PERFORMANCE],
[
AC_MSG_CHECKING([for curses performance tradeoff])
AC_CACHE_VAL(cf_cv_curs_performance,[
    cf_cv_curs_performance=no
    AC_TRY_COMPILE([
#include <${cf_cv_ncurses_header-curses.h}>],[
#if defined(wbkgdset) && defined(clearok) && defined(getbkgd)
	int x = ERR;
#else
	int x = ;	/* force an error */
#endif
	],[
	AC_TRY_COMPILE([
#define CURS_PERFORMANCE
#include <${cf_cv_ncurses_header-curses.h}>],[
#if defined(wbkgdset) && defined(clearok) && defined(getbkgd)
	int x = ;	/* force an error */
#else
	int x = ERR;
#endif
	],[cf_cv_curs_performance=yes])])])
AC_MSG_RESULT($cf_cv_curs_performance)
test $cf_cv_curs_performance = yes && AC_DEFINE(CURS_PERFORMANCE)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for a program in the given list $3, defining the corresponding
dnl program variable $2.
dnl
AC_DEFUN([CF_DEFINE_PROG],[
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(cf_cv_$2,[
	cf_cv_$2=unknown
	for cv_path in $3
	do
		if test -f $cv_path ; then
			cf_cv_$2=$cv_path
			break
		fi
	done
	])
AC_MSG_RESULT($cf_cv_$2)
AC_DEFINE_UNQUOTED($2,"$cf_cv_$2")
])
dnl ---------------------------------------------------------------------------
dnl You can always use "make -n" to see the actual options, but it's hard to
dnl pick out/analyze warning messages when the compile-line is long.
dnl
dnl Sets:
dnl	ECHO_LD - symbol to prefix "cc -o" lines
dnl	RULE_CC - symbol to put before implicit "cc -c" lines (e.g., .c.o)
dnl	SHOW_CC - symbol to put before explicit "cc -c" lines
dnl	ECHO_CC - symbol to put before any "cc" line
dnl
AC_DEFUN([CF_DISABLE_ECHO],[
AC_MSG_CHECKING(if you want to see long compiling messages)
CF_ARG_DISABLE(echo,
	[  --disable-echo          test: display "compiling" commands],
	[
    ECHO_LD='@echo linking [$]@;'
    RULE_CC='	@echo compiling [$]<'
    SHOW_CC='	@echo compiling [$]@'
    ECHO_CC='@'
],[
    ECHO_LD=''
    RULE_CC='# compiling'
    SHOW_CC='# compiling'
    ECHO_CC=''
])
AC_MSG_RESULT($enableval)
AC_SUBST(ECHO_LD)
AC_SUBST(RULE_CC)
AC_SUBST(SHOW_CC)
AC_SUBST(ECHO_CC)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check whether character set is EBCDIC.
AC_DEFUN([CF_EBCDIC],[
AC_MSG_CHECKING(if character set is EBCDIC)
AC_CACHE_VAL(cf_cv_ebcdic,[
	AC_TRY_COMPILE([ ],
[ /* TryCompile function for CharSet.
   Treat any failure as ASCII for compatibility with existing art.
   Use compile-time rather than run-time tests for cross-compiler
   tolerance.  */
#if '0'!=240
make an error "Character set is not EBCDIC"
#endif ],
[ # TryCompile action if true
cf_cv_ebcdic=yes ],
[ # TryCompile action if false
cf_cv_ebcdic=no])
# end of TryCompile ])
# end of CacheVal CvEbcdic
AC_MSG_RESULT($cf_cv_ebcdic)
case "$cf_cv_ebcdic" in  #(vi
    yes) AC_DEFINE(EBCDIC)
         AC_DEFINE(NOT_ASCII);; #(vi
    *)   ;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if 'errno' is declared in <errno.h>
AC_DEFUN([CF_ERRNO],
[
CF_CHECK_ERRNO(errno)
])dnl
dnl ---------------------------------------------------------------------------
AC_DEFUN([CF_FANCY_CURSES],
[
AC_MSG_CHECKING(if curses supports fancy attributes)
AC_CACHE_VAL(cf_cv_fancy_curses,[
	AC_TRY_LINK([
#include <${cf_cv_ncurses_header-curses.h}>
],
	[attrset(A_UNDERLINE|A_BOLD|A_REVERSE);
	 wattrset(stdscr, A_BLINK|A_DIM);
	 attroff(A_BOLD);
	 keypad(stdscr,TRUE);
	],
	[cf_cv_fancy_curses=yes],
	[cf_cv_fancy_curses=no])
	])
AC_MSG_RESULT($cf_cv_fancy_curses)
test $cf_cv_fancy_curses = yes && AC_DEFINE(FANCY_CURSES)
])
dnl ---------------------------------------------------------------------------
dnl Look for a non-standard library, given parameters for AC_TRY_LINK.  We
dnl prefer a standard location, and use -L options only if we do not find the
dnl library in the standard library location(s).
dnl	$1 = library name
dnl	$2 = includes
dnl	$3 = code fragment to compile/link
dnl	$4 = corresponding function-name
dnl
dnl Sets the variable "$cf_libdir" as a side-effect, so we can see if we had
dnl to use a -L option.
AC_DEFUN([CF_FIND_LIBRARY],
[
	cf_cv_have_lib_$1=no
	cf_libdir=""
	AC_CHECK_FUNC($4,cf_cv_have_lib_$1=yes,[
		cf_save_LIBS="$LIBS"
		AC_MSG_CHECKING(for $4 in -l$1)
		LIBS="-l$1 $LIBS"
		AC_TRY_LINK([$2],[$3],
			[AC_MSG_RESULT(yes)
			 cf_cv_have_lib_$1=yes
			],
			[AC_MSG_RESULT(no)
			CF_LIBRARY_PATH(cf_search,$1)
			for cf_libdir in $cf_search
			do
				AC_MSG_CHECKING(for -l$1 in $cf_libdir)
				LIBS="-L$cf_libdir -l$1 $cf_save_LIBS"
				AC_TRY_LINK([$2],[$3],
					[AC_MSG_RESULT(yes)
			 		 cf_cv_have_lib_$1=yes
					 break],
					[AC_MSG_RESULT(no)
					 LIBS="$cf_save_LIBS"])
			done
			])
		])
if test $cf_cv_have_lib_$1 = no ; then
	AC_ERROR(Cannot link $1 library)
fi
case $host_os in #(vi
linux*) # Suse Linux does not follow /usr/lib convention
	LIBS="$LIBS -L/lib"
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for availability of fcntl versus ioctl(,FIONBIO,).  Lynx uses this
dnl for Sequent (ptx), and it is needed for OS/2 EMX.
AC_DEFUN([CF_FIONBIO],
[
AC_CACHE_CHECK(if we should use fcntl or ioctl,cf_cv_fionbio,[
AC_TRY_LINK([
#include <sys/types.h>
#include <sys/ioctl.h>
],[
        int ret = ioctl(0, FIONBIO, (char *)0);
	],[cf_cv_fionbio=ioctl],[
AC_TRY_LINK([
#include <sys/types.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif
#endif],[
        int ret = fcntl(0, F_SETFL, O_NONBLOCK);
	],
	[cf_cv_fionbio=fcntl],
	[cf_cv_fionbio=unknown])])
])
test "$cf_cv_fionbio" = "fcntl" && AC_DEFINE(USE_FCNTL)
])dnl
dnl ---------------------------------------------------------------------------
dnl A conventional existence-check for 'lstat' won't work with the Linux
dnl version of gcc 2.7.0, since the symbol is defined only within <sys/stat.h>
dnl as an inline function.
dnl
dnl So much for portability.
AC_DEFUN([CF_FUNC_LSTAT],
[
AC_MSG_CHECKING(for lstat)
AC_CACHE_VAL(ac_cv_func_lstat,[
AC_TRY_LINK([
#include <sys/types.h>
#include <sys/stat.h>],
	[lstat(".", (struct stat *)0)],
	[ac_cv_func_lstat=yes],
	[ac_cv_func_lstat=no])
	])
AC_MSG_RESULT($ac_cv_func_lstat )
if test $ac_cv_func_lstat = yes; then
	AC_DEFINE(HAVE_LSTAT)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Test for the presence of <sys/wait.h>, 'union wait', arg-type of 'wait()'
dnl and/or 'waitpid()'.
dnl
dnl Note that we cannot simply grep for 'union wait' in the wait.h file,
dnl because some Posix systems turn this on only when a BSD variable is
dnl defined.
dnl
dnl I don't use AC_HEADER_SYS_WAIT, because it defines HAVE_SYS_WAIT_H, which
dnl would conflict with an attempt to test that header directly.
dnl
AC_DEFUN([CF_FUNC_WAIT],
[
AC_REQUIRE([CF_UNION_WAIT])
if test $cf_cv_type_unionwait = yes; then

	AC_MSG_CHECKING(if union wait can be used as wait-arg)
	AC_CACHE_VAL(cf_cv_arg_union_wait,[
		AC_TRY_COMPILE($cf_wait_headers,
 			[union wait x; wait(&x)],
			[cf_cv_arg_union_wait=yes],
			[cf_cv_arg_union_wait=no])
		])
	AC_MSG_RESULT($cf_cv_arg_union_wait)
	test $cf_cv_arg_union_wait = yes && AC_DEFINE(WAIT_USES_UNION)

	AC_MSG_CHECKING(if union wait can be used as waitpid-arg)
	AC_CACHE_VAL(cf_cv_arg_union_waitpid,[
		AC_TRY_COMPILE($cf_wait_headers,
 			[union wait x; waitpid(0, &x, 0)],
			[cf_cv_arg_union_waitpid=yes],
			[cf_cv_arg_union_waitpid=no])
		])
	AC_MSG_RESULT($cf_cv_arg_union_waitpid)
	test $cf_cv_arg_union_waitpid = yes && AC_DEFINE(WAITPID_USES_UNION)

fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Test for availability of useful gcc __attribute__ directives to quiet
dnl compiler warnings.  Though useful, not all are supported -- and contrary
dnl to documentation, unrecognized directives cause older compilers to barf.
AC_DEFUN([CF_GCC_ATTRIBUTES],
[
if test -n "$GCC"
then
cat > conftest.i <<EOF
#ifndef GCC_PRINTF
#define GCC_PRINTF 0
#endif
#ifndef GCC_SCANF
#define GCC_SCANF 0
#endif
#ifndef GCC_NORETURN
#define GCC_NORETURN /* nothing */
#endif
#ifndef GCC_UNUSED
#define GCC_UNUSED /* nothing */
#endif
EOF
if test -n "$GCC"
then
	AC_CHECKING([for gcc __attribute__ directives])
	changequote(,)dnl
cat > conftest.$ac_ext <<EOF
#line __oline__ "configure"
#include "confdefs.h"
#include "conftest.h"
#include "conftest.i"
#if	GCC_PRINTF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var) /*nothing*/
#endif
#if	GCC_SCANF
#define GCC_SCANFLIKE(fmt,var)  __attribute__((format(scanf,fmt,var)))
#else
#define GCC_SCANFLIKE(fmt,var)  /*nothing*/
#endif
extern void wow(char *,...) GCC_SCANFLIKE(1,2);
extern void oops(char *,...) GCC_PRINTFLIKE(1,2) GCC_NORETURN;
extern void foo(void) GCC_NORETURN;
int main(int argc GCC_UNUSED, char *argv[] GCC_UNUSED) { return 0; }
EOF
	changequote([,])dnl
	for cf_attribute in scanf printf unused noreturn
	do
		CF_UPPER(CF_ATTRIBUTE,$cf_attribute)
		cf_directive="__attribute__(($cf_attribute))"
		echo "checking for gcc $cf_directive" 1>&AC_FD_CC
		case $cf_attribute in
		scanf|printf)
		cat >conftest.h <<EOF
#define GCC_$CF_ATTRIBUTE 1
EOF
			;;
		*)
		cat >conftest.h <<EOF
#define GCC_$CF_ATTRIBUTE $cf_directive
EOF
			;;
		esac
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... $cf_attribute)
			cat conftest.h >>confdefs.h
#		else
#			sed -e 's/__attr.*/\/*nothing*\//' conftest.h >>confdefs.h
		fi
	done
else
	fgrep define conftest.i >>confdefs.h
fi
rm -rf conftest*
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if the compiler supports useful warning options.  There's a few that
dnl we don't use, simply because they're too noisy:
dnl
dnl	-Wconversion (useful in older versions of gcc, but not in gcc 2.7.x)
dnl	-Wredundant-decls (system headers make this too noisy)
dnl	-Wtraditional (combines too many unrelated messages, only a few useful)
dnl	-Wwrite-strings (too noisy, but should review occasionally)
dnl	-pedantic
dnl
AC_DEFUN([CF_GCC_WARNINGS],
[
if test -n "$GCC"
then
	changequote(,)dnl
	cat > conftest.$ac_ext <<EOF
#line __oline__ "configure"
int main(int argc, char *argv[]) { return (argv[argc-1] == 0) ; }
EOF
	changequote([,])dnl
	AC_CHECKING([for gcc warning options])
	cf_save_CFLAGS="$CFLAGS"
	EXTRA_CFLAGS="-W -Wall"
	cf_warn_CONST=""
	test "$with_ext_const" = yes && cf_warn_CONST="Wwrite-strings"
	for cf_opt in \
		Wbad-function-cast \
		Wcast-align \
		Wcast-qual \
		Winline \
		Wmissing-declarations \
		Wmissing-prototypes \
		Wnested-externs \
		Wpointer-arith \
		Wshadow \
		Wstrict-prototypes $cf_warn_CONST
	do
		CFLAGS="$cf_save_CFLAGS $EXTRA_CFLAGS -$cf_opt"
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... -$cf_opt)
			EXTRA_CFLAGS="$EXTRA_CFLAGS -$cf_opt"
			test "$cf_opt" = Wcast-qual && EXTRA_CFLAGS="$EXTRA_CFLAGS -DXTSTRINGDEFINES"
		fi
	done
	rm -f conftest*
	CFLAGS="$cf_save_CFLAGS"
fi
AC_SUBST(EXTRA_CFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl Construct a search-list for a nonstandard header-file
AC_DEFUN([CF_HEADER_PATH],
[$1=""
if test -d "$includedir"  ; then
test "$includedir" != NONE       && $1="[$]$1 $includedir $includedir/$2"
fi
if test -d "$oldincludedir"  ; then
test "$oldincludedir" != NONE    && $1="[$]$1 $oldincludedir $oldincludedir/$2"
fi
if test -d "$prefix"; then
test "$prefix" != NONE           && $1="[$]$1 $prefix/include $prefix/include/$2"
fi
test "$prefix" != /usr/local     && $1="[$]$1 /usr/local/include /usr/local/include/$2"
test "$prefix" != /usr           && $1="[$]$1 /usr/include /usr/include/$2"
])dnl
dnl ---------------------------------------------------------------------------
dnl Insert text into the help-message, for readability, from AC_ARG_WITH.
AC_DEFUN([CF_HELP_MESSAGE],
[AC_DIVERT_HELP([$1])dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl For Lynx, check if the libraries we have found give us inet_aton, or
dnl inet_addr.  If not, try to find the latter function with -lbind or
dnl -lresolv, and put that on the end of the libraries, i.e., after the network
dnl libraries.
dnl
dnl FIXME: the inner cases will probably need work on the header files.
AC_DEFUN([CF_INET_ADDR],[
AC_CACHE_CHECK(for inet_aton function,cf_cv_have_inet_aton,[
AC_TRY_LINK([#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
],[inet_aton(0, (struct in_addr *)0)],
    [cf_cv_have_inet_aton=yes],
    [cf_cv_have_inet_aton=no])])
if test "$cf_cv_have_inet_aton" = yes ; then
    AC_DEFINE(HAVE_INET_ATON)
else
    AC_CACHE_CHECK(for inet_addr function,cf_cv_have_inet_addr,[
    AC_TRY_LINK([#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
    ],[inet_addr(0)],
	[cf_cv_have_inet_addr=yes],
	[cf_cv_have_inet_addr=no])])
    if test "$cf_cv_have_inet_addr" = no ; then
	AC_CACHE_CHECK(for library with inet_addr,cf_cv_lib_inet_addr,[
	    cf_save_LIBS="$LIBS"
	    for cf_inetlib in -lbind -lresolv
	    do
		LIBS="$cf_save_LIBS $cf_inetlib"
		AC_TRY_LINK([#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
		],[inet_addr(0)],
		    [cf_cv_lib_inet_addr=$cf_inetlib],
		    [cf_cv_lib_inet_addr=no])
		LIBS="$cf_save_LIBS"
		test "$cf_cv_lib_inet_addr" != no && break
	    done
	])
	if test "$cf_cv_lib_inet_addr" != no ; then
	    LIBS="$LIBS $cf_cv_lib_inet_addr"
	else
	    AC_MSG_WARN(Unable to find library for inet_addr function)
	fi
    fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Construct a search-list for a nonstandard library-file
AC_DEFUN([CF_LIBRARY_PATH],
[$1=""
if test -d "$libdir"  ; then
test "$libdir" != NONE           && $1="[$]$1 $libdir $libdir/$2"
fi
if test -d "$exec_prefix"; then
test "$exec_prefix" != NONE      && $1="[$]$1 $exec_prefix/lib $exec_prefix/lib/$2"
fi
if test -d "$prefix"; then
test "$prefix" != NONE           && \
test "$prefix" != "$exec_prefix" && $1="[$]$1 $prefix/lib $prefix/lib/$2"
fi
test "$prefix" != /usr/local     && $1="[$]$1 /usr/local/lib /usr/local/lib/$2"
test "$prefix" != /usr           && $1="[$]$1 /usr/lib /usr/lib/$2"
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we've got setlocale() and its header, <locale.h>
AC_DEFUN([CF_LOCALE],
[
AC_MSG_CHECKING(for setlocale())
AC_CACHE_VAL(cf_cv_locale,[
AC_TRY_LINK([#include <locale.h>],
	[setlocale(LC_ALL, "")],
	[cf_cv_locale=yes],
	[cf_cv_locale=no])
	])
AC_MSG_RESULT($cf_cv_locale)
test $cf_cv_locale = yes && AC_DEFINE(LOCALE)
])
dnl ---------------------------------------------------------------------------
dnl Check for pre-1.9.9g ncurses (among other problems, the most obvious is
dnl that color combinations don't work).
AC_DEFUN([CF_NCURSES_BROKEN],
[
AC_REQUIRE([CF_NCURSES_VERSION])
if test "$cf_cv_ncurses_version" != no ; then
AC_MSG_CHECKING(for obsolete/broken version of ncurses)
AC_CACHE_VAL(cf_cv_ncurses_broken,[
AC_TRY_COMPILE([
#include <${cf_cv_ncurses_header-curses.h}>],[
#if defined(NCURSES_VERSION) && defined(wgetbkgd)
	make an error
#else
	int x = 1
#endif
],
	[cf_cv_ncurses_broken=no],
	[cf_cv_ncurses_broken=yes])
])
AC_MSG_RESULT($cf_cv_ncurses_broken)
if test "$cf_cv_ncurses_broken" = yes ; then
	AC_MSG_WARN(hmm... you should get an up-to-date version of ncurses)
	AC_DEFINE(NCURSES_BROKEN)
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Look for the SVr4 curses clone 'ncurses' in the standard places, adjusting
dnl the CPPFLAGS variable.
dnl
dnl The header files may be installed as either curses.h, or ncurses.h
dnl (obsolete).  If not installed for overwrite, the curses.h file would be
dnl in an ncurses subdirectory (e.g., /usr/include/ncurses), but someone may
dnl have installed overwriting the vendor's curses.  Only very old versions
dnl (pre-1.9.2d, the first autoconf'd version) of ncurses don't define
dnl either __NCURSES_H or NCURSES_VERSION in the header.
dnl
dnl If the installer has set $CFLAGS or $CPPFLAGS so that the ncurses header
dnl is already in the include-path, don't even bother with this, since we cannot
dnl easily determine which file it is.  In this case, it has to be <curses.h>.
dnl
AC_DEFUN([CF_NCURSES_CPPFLAGS],
[
AC_MSG_CHECKING(for ncurses header file)
AC_CACHE_VAL(cf_cv_ncurses_header,[
	AC_TRY_COMPILE([#include <curses.h>],[
#ifdef NCURSES_VERSION
printf("%s\n", NCURSES_VERSION);
#else
#ifdef __NCURSES_H
printf("old\n");
#else
make an error
#endif
#endif
	],
	[cf_cv_ncurses_header=predefined],[
	CF_HEADER_PATH(cf_search,ncurses)
	test -n "$verbose" && echo
	for cf_incdir in $cf_search
	do
		for cf_header in \
			curses.h \
			ncurses.h
		do
changequote(,)dnl
			if egrep "NCURSES_[VH]" $cf_incdir/$cf_header 1>&AC_FD_CC 2>&1; then
changequote([,])dnl
				cf_cv_ncurses_header=$cf_incdir/$cf_header
				test -n "$verbose" && echo $ac_n "	... found $ac_c" 1>&AC_FD_MSG
				break
			fi
			test -n "$verbose" && echo "	... tested $cf_incdir/$cf_header" 1>&AC_FD_MSG
		done
		test -n "$cf_cv_ncurses_header" && break
	done
	test -z "$cf_cv_ncurses_header" && AC_ERROR(not found)
	])])
AC_MSG_RESULT($cf_cv_ncurses_header)
AC_DEFINE(NCURSES)

changequote(,)dnl
cf_incdir=`echo $cf_cv_ncurses_header | sed -e 's:/[^/]*$::'`
changequote([,])dnl

case $cf_cv_ncurses_header in # (vi
*/ncurses.h)
	AC_DEFINE(HAVE_NCURSES_H)
	;;
esac

case $cf_cv_ncurses_header in # (vi
predefined) # (vi
	cf_cv_ncurses_header=curses.h
	;;
*)
	CF_ADD_INCDIR($cf_incdir)
	;;
esac
CF_NCURSES_VERSION
])dnl
dnl ---------------------------------------------------------------------------
dnl Look for the ncurses library.  This is a little complicated on Linux,
dnl because it may be linked with the gpm (general purpose mouse) library.
dnl Some distributions have gpm linked with (bsd) curses, which makes it
dnl unusable with ncurses.  However, we don't want to link with gpm unless
dnl ncurses has a dependency, since gpm is normally set up as a shared library,
dnl and the linker will record a dependency.
AC_DEFUN([CF_NCURSES_LIBS],
[AC_REQUIRE([CF_NCURSES_CPPFLAGS])

	# This works, except for the special case where we find gpm, but
	# ncurses is in a nonstandard location via $LIBS, and we really want
	# to link gpm.
cf_ncurses_LIBS=""
cf_ncurses_SAVE="$LIBS"
AC_CHECK_LIB(gpm,Gpm_Open,
	[AC_CHECK_LIB(gpm,initscr,
		[LIBS="$cf_ncurses_SAVE"],
		[cf_ncurses_LIBS="-lgpm"])])

case $host_os in #(vi
freebsd*)
	# This is only necessary if you are linking against an obsolete
	# version of ncurses (but it should do no harm, since it's static).
	AC_CHECK_LIB(mytinfo,tgoto,[cf_ncurses_LIBS="-lmytinfo $cf_ncurses_LIBS"])
	;;
esac

LIBS="$cf_ncurses_LIBS $LIBS"
CF_FIND_LIBRARY(ncurses,
	[#include <${cf_cv_ncurses_header-curses.h}>],
	[initscr()],
	initscr)

if test -n "$cf_ncurses_LIBS" ; then
	AC_MSG_CHECKING(if we can link ncurses without $cf_ncurses_LIBS)
	cf_ncurses_SAVE="$LIBS"
	for p in $cf_ncurses_LIBS ; do
		q=`echo $LIBS | sed -e 's/'$p' //' -e 's/'$p'$//'`
		if test "$q" != "$LIBS" ; then
			LIBS="$q"
		fi
	done
	AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
		[initscr(); mousemask(0,0); tgoto((char *)0, 0, 0);],
		[AC_MSG_RESULT(yes)],
		[AC_MSG_RESULT(no)
		 LIBS="$cf_ncurses_SAVE"])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for the version of ncurses, to aid in reporting bugs, etc.
AC_DEFUN([CF_NCURSES_VERSION],
[AC_MSG_CHECKING(for ncurses version)
AC_CACHE_VAL(cf_cv_ncurses_version,[
	cf_cv_ncurses_version=no
	cf_tempfile=out$$
	AC_TRY_RUN([
#include <${cf_cv_ncurses_header-curses.h}>
int main()
{
	FILE *fp = fopen("$cf_tempfile", "w");
#ifdef NCURSES_VERSION
# ifdef NCURSES_VERSION_PATCH
	fprintf(fp, "%s.%d\n", NCURSES_VERSION, NCURSES_VERSION_PATCH);
# else
	fprintf(fp, "%s\n", NCURSES_VERSION);
# endif
#else
# ifdef __NCURSES_H
	fprintf(fp, "old\n");
# else
	make an error
# endif
#endif
	exit(0);
}],[
	cf_cv_ncurses_version=`cat $cf_tempfile`
	rm -f $cf_tempfile],,[

	# This will not work if the preprocessor splits the line after the
	# Autoconf token.  The 'unproto' program does that.
	cat > conftest.$ac_ext <<EOF
#include <${cf_cv_ncurses_header-curses.h}>
#undef Autoconf
#ifdef NCURSES_VERSION
Autoconf NCURSES_VERSION
#else
#ifdef __NCURSES_H
Autoconf "old"
#endif
;
#endif
EOF
	cf_try="$ac_cpp conftest.$ac_ext 2>&AC_FD_CC | grep '^Autoconf ' >conftest.out"
	AC_TRY_EVAL(cf_try)
	if test -f conftest.out ; then
changequote(,)dnl
		cf_out=`cat conftest.out | sed -e 's@^Autoconf @@' -e 's@^[^"]*"@@' -e 's@".*@@'`
changequote([,])dnl
		test -n "$cf_out" && cf_cv_ncurses_version="$cf_out"
		rm -f conftest.out
	fi
])])
AC_MSG_RESULT($cf_cv_ncurses_version)
])
dnl ---------------------------------------------------------------------------
dnl After checking for functions in the default $LIBS, make a further check
dnl for the functions that are netlib-related (these aren't always in the
dnl libc, etc., and have to be handled specially because there are conflicting
dnl and broken implementations.
dnl Common library requirements (in order):
dnl	-lresolv -lsocket -lnsl
dnl	-lnsl -lsocket
dnl	-lsocket
dnl	-lbsd
AC_DEFUN([CF_NETLIBS],[
cf_test_netlibs=no
AC_MSG_CHECKING(for network libraries)
AC_CACHE_VAL(cf_cv_netlibs,[
AC_MSG_RESULT(working...)
cf_cv_netlibs=""
cf_test_netlibs=yes
AC_CHECK_FUNCS(gethostname,,[
	CF_RECHECK_FUNC(gethostname,nsl,cf_cv_netlibs,[
		CF_RECHECK_FUNC(gethostname,socket,cf_cv_netlibs)])])
#
# FIXME:  sequent needs this library (i.e., -lsocket -linet -lnsl), but
# I don't know the entrypoints - 97/7/22 TD
AC_HAVE_LIBRARY(inet,cf_cv_netlibs="-linet $cf_cv_netlibs")
#
if test "$ac_cv_func_lsocket" != no ; then
AC_CHECK_FUNCS(socket,,[
	CF_RECHECK_FUNC(socket,socket,cf_cv_netlibs,[
		CF_RECHECK_FUNC(socket,bsd,cf_cv_netlibs)])])
fi
#
AC_CHECK_FUNCS(gethostbyname,,[
	CF_RECHECK_FUNC(gethostbyname,nsl,cf_cv_netlibs)])
#
AC_CHECK_FUNCS(strcasecmp,,[
	CF_RECHECK_FUNC(strcasecmp,resolv,cf_cv_netlibs)])
])
LIBS="$LIBS $cf_cv_netlibs"
test $cf_test_netlibs = no && echo "$cf_cv_netlibs" >&AC_FD_MSG
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for the symbol NGROUPS
AC_DEFUN([CF_NGROUPS],
[
AC_MSG_CHECKING(if NGROUPS is defined)
AC_CACHE_VAL(cf_cv_ngroups,[
AC_TRY_COMPILE([
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
],[int x = NGROUPS],
	[cf_cv_ngroups=yes],
	[AC_TRY_COMPILE([
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
],[int x = NGROUPS_MAX],
		[cf_cv_ngroups=NGROUPS_MAX],
		[cf_cv_ngroups=no])
	])
AC_MSG_RESULT($cf_cv_ngroups)
if test "$cf_cv_ngroups" = no ; then
	AC_DEFINE(NGROUPS,16)
elif test "$cf_cv_ngroups" = NGROUPS_MAX ; then
	AC_DEFINE(NGROUPS,NGROUPS_MAX)
fi
])
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we use the messages included with this program
AC_DEFUN([CF_OUR_MESSAGES],
[
use_our_messages=no
if test -d $srcdir/po ; then
AC_MSG_CHECKING(if we should use included message-library)
	AC_ARG_ENABLE(included-msgs,
	[  --enable-included-msgs  use included messages, for i18n support],
	[use_our_messages=$enableval],
	[use_our_messages=yes])
fi
AC_MSG_RESULT($use_our_messages)
test $use_our_messages = yes && USE_OUR_MESSAGES=
AC_SUBST(USE_OUR_MESSAGES)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for a given program, defining corresponding symbol.
dnl	$1 = environment variable, which is suffixed by "_PATH" in the #define.
dnl	$2 = program name to find.
dnl
dnl If there is more than one token in the result, #define the remaining tokens
dnl to $1_ARGS.  We need this for 'install' in particular.
dnl
dnl FIXME: we should allow this to be overridden by environment variables
dnl
AC_DEFUN([CF_PATH_PROG],[
test -z "[$]$1" && $1=$2
if test "$with_full_paths" = yes ; then
	AC_PATH_PROG($1,$2,[$]$1)
else
	AC_MSG_CHECKING(for $2)
	AC_MSG_RESULT([$]$1)
	eval 'ac_cv_path_'$1'="'$2'"'
fi
cf_path_prog=""
cf_path_args=""
IFS="${IFS= 	}"; cf_save_ifs="$IFS"
case $host_os in #(vi
os2*) #(vi
	IFS="${IFS};"
	;;
*)
	IFS="${IFS}:"
	;;
esac
for cf_temp in $ac_cv_path_$1
do
	if test -z "$cf_path_prog" ; then
		cf_path_prog="$cf_temp"
	elif test -z "$cf_path_args" ; then
		cf_path_args="$cf_temp"
	else
		cf_path_args="$cf_path_args $cf_temp"
	fi
done
IFS="$cf_save_ifs"

AC_DEFINE_UNQUOTED($1_PATH,"$cf_path_prog")
test -n "$cf_path_args" && AC_DEFINE_UNQUOTED($1_ARGS,"$cf_path_args")
])dnl
dnl ---------------------------------------------------------------------------
dnl Check the argument to see that it looks like a pathname.  Rewrite it if it
dnl begins with one of the prefix/exec_prefix variables, and then again if the
dnl result begins with 'NONE'.  This is necessary to workaround autoconf's
dnl delayed evaluation of those symbols.
AC_DEFUN([CF_PATH_SYNTAX],[
case ".[$]$1" in #(vi
./*) #(vi
  ;;
.\[$]{*prefix}*) #(vi
  eval $1="[$]$1"
  case ".[$]$1" in #(vi
  .NONE/*)
    $1=`echo [$]$1 | sed -e s@NONE@$ac_default_prefix@`
    ;;
  esac
  ;; #(vi
.NONE/*)
  $1=`echo [$]$1 | sed -e s@NONE@$ac_default_prefix@`
  ;;
*)
  AC_ERROR(expected a pathname)
  ;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl Re-check on a function to see if we can pick it up by adding a library.
dnl	$1 = function to check
dnl	$2 = library to check in
dnl	$3 = environment to update (e.g., $LIBS)
dnl	$4 = what to do if this fails
dnl
dnl This uses 'unset' if the shell happens to support it, but leaves the
dnl configuration variable set to 'unknown' if not.  This is a little better
dnl than the normal autoconf test, which gives misleading results if a test
dnl for the function is made (e.g., with AC_CHECK_FUNC) after this macro is
dnl used (autoconf does not distinguish between a null token and one that is
dnl set to 'no').
AC_DEFUN([CF_RECHECK_FUNC],[
AC_CHECK_LIB($2,$1,[
	CF_UPPER(cf_tr_func,$1)
	AC_DEFINE_UNQUOTED(HAVE_$cf_tr_func)
	ac_cv_func_$1=yes
	$3="-l$2 [$]$3"],[
	ac_cv_func_$1=unknown
	unset ac_cv_func_$1 2>/dev/null
	$4],
	[[$]$3])
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for broken definition of 'remove()'.  This is (in particular) broken
dnl on the older version of SCO (I'd cite version if I knew where to look) by
dnl having <stdio.h> #define remove to __unlink, which appears in no library.
dnl
dnl Fortuitously, we can combine this with a more general test: do we have
dnl 'unlink()' but no 'remove()'.  Note, however, that we cannot simply #define
dnl remove to unlink, but have to make a fallback function.
dnl
AC_DEFUN([CF_REMOVE_BROKEN],
[
AC_MSG_CHECKING(for broken/missing definition of remove)
AC_CACHE_VAL(cf_cv_baddef_remove,[
AC_TRY_LINK(
	[#include <stdio.h>],
	[remove("dummy")],
	[cf_cv_baddef_remove=no],
	[AC_TRY_LINK(
		[#include <stdio.h>
		int __unlink(name) { return unlink(name); } ],
		[remove("dummy")],
		[cf_cv_baddef_remove=yes],
		[cf_cv_baddef_remove=unknown])
	])
])
AC_MSG_RESULT($cf_cv_baddef_remove)
test "$cf_cv_baddef_remove" != no && AC_DEFINE(NEED_REMOVE)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for definitions & structures needed for window size-changing
dnl FIXME: check that this works with "snake" (HP-UX 10.x)
AC_DEFUN([CF_SIZECHANGE],
[
AC_MSG_CHECKING([declaration of size-change])
AC_CACHE_VAL(cf_cv_sizechange,[
    cf_cv_sizechange=unknown
    cf_save_CFLAGS="$CFLAGS"

for cf_opts in "" "NEED_PTEM_H"
do

    CFLAGS="$cf_save_CFLAGS"
    test -n "$cf_opts" && CFLAGS="$CFLAGS -D$cf_opts"
    AC_TRY_COMPILE([#include <sys/types.h>
#if HAVE_TERMIOS_H
#include <termios.h>
#else
#if HAVE_TERMIO_H
#include <termio.h>
#endif
#endif
#if NEED_PTEM_H
/* This is a workaround for SCO:  they neglected to define struct winsize in
 * termios.h -- it's only in termio.h and ptem.h
 */
#include        <sys/stream.h>
#include        <sys/ptem.h>
#endif
#if !defined(sun) || !defined(HAVE_TERMIOS_H)
#include <sys/ioctl.h>
#endif
],[
#ifdef TIOCGSIZE
	struct ttysize win;	/* FIXME: what system is this? */
	int y = win.ts_lines;
	int x = win.ts_cols;
#else
#ifdef TIOCGWINSZ
	struct winsize win;
	int y = win.ws_row;
	int x = win.ws_col;
#else
	no TIOCGSIZE or TIOCGWINSZ
#endif /* TIOCGWINSZ */
#endif /* TIOCGSIZE */
	],
	[cf_cv_sizechange=yes],
	[cf_cv_sizechange=no])

	CFLAGS="$cf_save_CFLAGS"
	if test "$cf_cv_sizechange" = yes ; then
		echo "size-change succeeded ($cf_opts)" >&AC_FD_CC
		test -n "$cf_opts" && AC_DEFINE_UNQUOTED($cf_opts)
		break
	fi
done
	])
AC_MSG_RESULT($cf_cv_sizechange)
test $cf_cv_sizechange != no && AC_DEFINE(HAVE_SIZECHANGE)
])dnl
dnl ---------------------------------------------------------------------------
dnl Look for the slang header files in the standard places, adjusting the
dnl CPPFLAGS variable.
dnl
AC_DEFUN([CF_SLANG_CPPFLAGS],
[
AC_MSG_CHECKING(for slang header file)
AC_CACHE_VAL(cf_cv_slang_header,[
	AC_TRY_COMPILE([#include <slang.h>],
	[printf("%s\n", SLANG_VERSION)],
	[cf_cv_slang_header=predefined],[
	CF_HEADER_PATH(cf_search,slang)
	for cf_incdir in $cf_search
	do
		for cf_header in \
			slang.h
		do
			echo trying $cf_incdir/$cf_header 1>&AC_FD_CC
			if egrep "SLANG_VERSION" $cf_incdir/$cf_header 1>&AC_FD_CC 2>&1; then
				cf_cv_slang_header=$cf_incdir/$cf_header
				break
			fi
		done
		test -n "$cf_cv_slang_header" && break
	done
	test -z "$cf_cv_slang_header" && AC_ERROR(not found)
	])])
AC_MSG_RESULT($cf_cv_slang_header)
AC_DEFINE(USE_SLANG)

changequote(,)dnl
cf_incdir=`echo $cf_cv_slang_header | sed -e 's:/[^/]*$::'`
changequote([,])dnl

case $cf_cv_slang_header in # (vi
predefined) # (vi
	;;
*)
	CF_ADD_INCDIR($cf_incdir)
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl Look for the slang library.
AC_DEFUN([CF_SLANG_LIBS],
[
cf_slang_LIBS1="$LIBS"
CF_TERMCAP_LIBS
cf_slang_LIBS2="$LIBS"
AC_CHECK_FUNC(acos,,[CF_RECHECK_FUNC(acos,m,LIBS)])
CF_FIND_LIBRARY(slang,
	[#include <slang.h>],
	[SLtt_get_screen_size()],
	SLtt_get_screen_size)
cf_slang_LIBS3="$LIBS"
AC_MSG_CHECKING(if we can link slang without termcap)
if test -n "`echo $cf_slang_LIBS1 | sed -e 's/ //g'`" ; then
	cf_exclude=`echo ".$cf_slang_LIBS2" | sed -e "s@$cf_slang_LIBS1@@" -e 's@^.@@'`
else
	cf_exclude="$cf_slang_LIBS2"
fi
LIBS=`echo ".$cf_slang_LIBS3" | sed -e "s@$cf_exclude@@" -e 's@^.@@'`
AC_TRY_LINK([#include <slang.h>],
	[SLtt_get_screen_size()],
	[cf_result=yes],
	[cf_result=no])
AC_MSG_RESULT($cf_result)
test $cf_result = no && LIBS="$cf_slang_LIBS3"
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for socks library
dnl $1 = the [optional] directory in which the library may be found
dnl $2 = the [optional] name of the library
AC_DEFUN([CF_SOCKS],[
case "$1" in #(vi
no|yes) #(vi
  ;;
*)
  LIBS="$LIBS -L$1"
  ;;
esac
LIBS="$LIBS -lsocks"
AC_DEFINE(SOCKS)
AC_DEFINE(accept,Raccept)
AC_DEFINE(bind,Rbind)
AC_DEFINE(connect,Rconnect)
AC_DEFINE(getpeername,Rgetpeername)
AC_DEFINE(getsockname,Rgetsockname)
AC_DEFINE(listen,Rlisten)
AC_DEFINE(recvfrom,Rrecvfrom)
AC_DEFINE(select,Rselect)
AC_TRY_LINK([
#include <stdio.h>],[
	accept((char *)0)],,
	[AC_ERROR(Cannot link with socks library)])
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for socks5 configuration
dnl $1 = the [optional] directory in which the library may be found
AC_DEFUN([CF_SOCKS5],[
case "$1" in #(vi
no|yes) #(vi
  ;;
*)
  LIBS="$LIBS -L$1"
  CFLAGS="$CFLAGS -I$1/../include"
  ;;
esac
LIBS="$LIBS -lsocks5"
AC_DEFINE(USE_SOCKS5)
AC_DEFINE(SOCKS)
AC_MSG_CHECKING(if the socks library uses socks4 prefix)
AC_TRY_LINK([
#include <socks.h>],[
	Rinit((char *)0)],
	[AC_DEFINE(USE_SOCKS4_PREFIX)
	 cf_use_socks4=yes],
	[AC_TRY_LINK([#include <socks.h>],
		[SOCKSinit((char *)0)],
		[cf_use_socks4=no],
		[AC_ERROR(Cannot link with socks5 library)])])
AC_MSG_RESULT($cf_use_socks4)
if test "$cf_use_socks4" = "yes" ; then
	AC_DEFINE(accept,Raccept)
	AC_DEFINE(bind,Rbind)
	AC_DEFINE(connect,Rconnect)
	AC_DEFINE(getpeername,Rgetpeername)
	AC_DEFINE(getsockname,Rgetsockname)
	AC_DEFINE(listen,Rlisten)
	AC_DEFINE(recvfrom,Rrecvfrom)
	AC_DEFINE(select,Rselect)
else
	AC_DEFINE(accept,SOCKSaccept)
	AC_DEFINE(getpeername,SOCKSgetpeername)
	AC_DEFINE(getsockname,SOCKSgetsockname)
	AC_DEFINE(recvfrom,SOCKSrecvfrom)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl	Remove "-g" option from the compiler options
AC_DEFUN([CF_STRIP_G_OPT],
[$1=`echo ${$1} | sed -e 's/-g //' -e 's/-g$//'`])dnl
dnl ---------------------------------------------------------------------------
dnl	Remove "-O" option from the compiler options
AC_DEFUN([CF_STRIP_O_OPT],[
changequote(,)dnl
$1=`echo ${$1} | sed -e 's/-O[1-9]\? //' -e 's/-O[1-9]\?$//'`
changequote([,])dnl
])dnl
dnl ---------------------------------------------------------------------------
AC_DEFUN([CF_SYSTEM_MAIL_FLAGS],
[
AC_MSG_CHECKING([system mail flags])
AC_CACHE_VAL(cf_cv_system_mail_flags,[
	case $cf_cv_SYSTEM_MAIL in
	*/mmdf/*)
		[cf_cv_system_mail_flags="-mlruxto,cc\\\\*"]
        	;;
	*)
        	[cf_cv_system_mail_flags="-t -oi"]
	esac
	])
AC_MSG_RESULT($cf_cv_system_mail_flags)
AC_DEFINE_UNQUOTED(SYSTEM_MAIL_FLAGS, "$cf_cv_system_mail_flags")
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for declaration of sys_nerr and sys_errlist in one of stdio.h and
dnl errno.h.  Declaration of sys_errlist on BSD4.4 interferes with our
dnl declaration.  Reported by Keith Bostic.
AC_DEFUN([CF_SYS_ERRLIST],
[
for cf_name in sys_nerr sys_errlist
do
    CF_CHECK_ERRNO($cf_name)
done
])dnl
dnl ---------------------------------------------------------------------------
dnl Look for termcap libraries, or the equivalent in terminfo.
AC_DEFUN([CF_TERMCAP_LIBS],
[
AC_CACHE_VAL(cf_cv_termlib,[
cf_cv_termlib=none
AC_TRY_LINK([],[char *x=(char*)tgoto("",0,0)],
[AC_TRY_LINK([],[int x=tigetstr("")],
	[cf_cv_termlib=terminfo],
	[cf_cv_termlib=termcap])
	CF_VERBOSE(using functions in predefined $cf_cv_termlib LIBS)
],[
ifelse([$1],,,[
if test "$1" = ncurses; then
	CF_NCURSES_CPPFLAGS
	CF_NCURSES_LIBS
	cf_cv_termlib=terminfo
fi
])
# HP-UX 9.x terminfo has setupterm, but no tigetstr.
if test "$cf_cv_termlib" = none; then
	AC_CHECK_LIB(termlib, tigetstr, [LIBS="$LIBS -ltermlib" cf_cv_termlib=terminfo])
fi
if test "$cf_cv_termlib" = none; then
	AC_CHECK_LIB(termlib, tgoto, [LIBS="$LIBS -ltermlib" cf_cv_termlib=termcap])
fi
if test "$cf_cv_termlib" = none; then
	# allow curses library for broken AIX system.
	AC_CHECK_LIB(curses, initscr, [LIBS="$LIBS -lcurses" cf_cv_termlib=termcap])
	AC_CHECK_LIB(termcap, tgoto, [LIBS="$LIBS -ltermcap" cf_cv_termlib=termcap])
fi
if test "$cf_cv_termlib" = none; then
	AC_CHECK_LIB(termcap, tgoto, [LIBS="$LIBS -ltermcap" cf_cv_termlib=termcap])
fi
if test "$cf_cv_termlib" = none; then
	AC_CHECK_LIB(ncurses, tgoto, [LIBS="$LIBS -lncurses" cf_cv_termlib=ncurses])
fi
])
if test "$cf_cv_termlib" = none; then
	AC_ERROR([Can't find -ltermlib, -lcurses, or -ltermcap])
fi
])])dnl
dnl ---------------------------------------------------------------------------
dnl Check if including termio.h with <curses.h> dies like on sysv68
dnl FIXME: this is too Lynx-specific
AC_DEFUN([CF_TERMIO_AND_CURSES],
[
AC_CACHE_CHECK(if we can include termio.h with curses,cf_cv_termio_and_curses,[
    cf_save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -DHAVE_CONFIG_H -I. -I${srcdir-.} -I${srcdir-.}/src -I${srcdir-.}/WWW/Library/Implementation"
    touch lynx_cfg.h
    AC_TRY_COMPILE([
#include <$1>
#include <termio.h>],
    [putchar(0x0a)],
    [cf_cv_termio_and_curses=yes],
    [cf_cv_termio_and_curses=no])
    CFLAGS="$cf_save_CFLAGS"
    rm -f lynx_cfg.h
])

test $cf_cv_termio_and_curses = yes && AC_DEFINE(TERMIO_AND_CURSES)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if including both termio.h and termios.h die like on DG.UX
AC_DEFUN([CF_TERMIO_AND_TERMIOS],
[
AC_MSG_CHECKING([termio.h and termios.h])
AC_CACHE_VAL(cf_cv_termio_and_termios,[
    AC_TRY_COMPILE([
#if HAVE_TERMIO_H
#include <termio.h>
#endif
#if HAVE_TERMIOS_H
#include <termios.h>
#endif ],
    [putchar (0x0a)],
    [cf_cv_termio_and_termios=yes],
    [cf_cv_termio_and_termios=no])])
AC_MSG_RESULT($cf_cv_termio_and_termios)
test $cf_cv_termio_and_termios = no && AC_DEFINE(TERMIO_AND_TERMIOS)
])dnl
dnl ---------------------------------------------------------------------------
AC_DEFUN([CF_TTYTYPE],
[
AC_MSG_CHECKING(if ttytype is declared in curses library)
AC_CACHE_VAL(cf_cv_have_ttytype,[
	AC_TRY_LINK([#include <${cf_cv_ncurses_header-curses.h}>],
	[char *x = &ttytype[1]; *x = 1],
	[cf_cv_have_ttytype=yes],
	[cf_cv_have_ttytype=no])
	])
AC_MSG_RESULT($cf_cv_have_ttytype)
test $cf_cv_have_ttytype = yes && AC_DEFINE(HAVE_TTYTYPE)
])
dnl ---------------------------------------------------------------------------
dnl Check to see if the BSD-style union wait is declared.  Some platforms may
dnl use this, though it is deprecated in favor of the 'int' type in Posix.
dnl Some vendors provide a bogus implementation that declares union wait, but
dnl uses the 'int' type instead; we try to spot these by checking for the
dnl associated macros.
dnl
dnl Ahem.  Some implementers cast the status value to an int*, as an attempt to
dnl use the macros for either union wait or int.  So we do a check compile to
dnl see if the macros are defined and apply to an int.
dnl
dnl Sets: $cf_cv_type_unionwait
dnl Defines: HAVE_TYPE_UNIONWAIT
AC_DEFUN([CF_UNION_WAIT],
[
AC_REQUIRE([CF_WAIT_HEADERS])
AC_MSG_CHECKING([for union wait])
AC_CACHE_VAL(cf_cv_type_unionwait,[
	AC_TRY_LINK($cf_wait_headers,
	[int x;
	 int y = WEXITSTATUS(x);
	 int z = WTERMSIG(x);
	 wait(&x);
	],
	[cf_cv_type_unionwait=no
	 echo compiles ok w/o union wait 1>&AC_FD_CC
	],[
	AC_TRY_LINK($cf_wait_headers,
	[union wait x;
#ifdef WEXITSTATUS
	 int y = WEXITSTATUS(x);
#endif
#ifdef WTERMSIG
	 int z = WTERMSIG(x);
#endif
	 wait(&x);
	],
	[cf_cv_type_unionwait=yes
	 echo compiles ok with union wait and possibly macros too 1>&AC_FD_CC
	],
	[cf_cv_type_unionwait=no])])])
AC_MSG_RESULT($cf_cv_type_unionwait)
test $cf_cv_type_unionwait = yes && AC_DEFINE(HAVE_TYPE_UNIONWAIT)
])dnl
dnl ---------------------------------------------------------------------------
dnl Make an uppercase version of a variable
dnl $1=uppercase($2)
AC_DEFUN([CF_UPPER],
[
changequote(,)dnl
$1=`echo $2 | tr '[a-z]' '[A-Z]'`
changequote([,])dnl
])dnl
dnl ---------------------------------------------------------------------------
AC_DEFUN([CF_UTMP],
[
AC_MSG_CHECKING(if struct utmp is declared)
AC_CACHE_VAL(cf_cv_have_utmp,[
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>],
	[struct utmp x; char *y = &x.ut_host[0]],
	[cf_cv_have_utmp=yes],
	[AC_TRY_COMPILE([#include <utmpx.h>],
		[struct utmpx x; char *y = &x.ut_host[0]],
		[cf_cv_have_utmp=utmpx],
		[cf_cv_have_utmp=no])
		])
	])
AC_MSG_RESULT($cf_cv_have_utmp)
test $cf_cv_have_utmp != no && AC_DEFINE(HAVE_UTMP)
test $cf_cv_have_utmp = utmpx && AC_DEFINE(UTMPX_FOR_UTMP)
])
dnl ---------------------------------------------------------------------------
dnl Check for ANSI stdarg.h vs varargs.h.  Note that some systems include
dnl <varargs.h> within <stdarg.h>.
AC_DEFUN([CF_VARARGS],
[
AC_CHECK_HEADERS(stdarg.h varargs.h)
AC_MSG_CHECKING(for standard varargs)
AC_CACHE_VAL(cf_cv_ansi_varargs,[
	AC_TRY_COMPILE([
#if HAVE_STDARG_H
#include <stdarg.h>
#else
#if HAVE_VARARGS_H
#include <varargs.h>
#endif
#endif
		],
		[return 0;} int foo(char *fmt,...){va_list args;va_start(args,fmt);va_end(args)],
		[cf_cv_ansi_varargs=yes],
		[cf_cv_ansi_varargs=no])
	])
AC_MSG_RESULT($cf_cv_ansi_varargs)
test $cf_cv_ansi_varargs = yes && AC_DEFINE(ANSI_VARARGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl Use AC_VERBOSE w/o the warnings
AC_DEFUN([CF_VERBOSE],
[test -n "$verbose" && echo "	$1" 1>&AC_FD_MSG
])dnl
dnl ---------------------------------------------------------------------------
dnl Build up an expression $cf_wait_headers with the header files needed to
dnl compile against the prototypes for 'wait()', 'waitpid()', etc.  Assume it's
dnl Posix, which uses <sys/types.h> and <sys/wait.h>, but allow SVr4 variation
dnl with <wait.h>.
AC_DEFUN([CF_WAIT_HEADERS],
[
AC_HAVE_HEADERS(sys/wait.h)
cf_wait_headers="#include <sys/types.h>
"
if test $ac_cv_header_sys_wait_h = yes; then
cf_wait_headers="$cf_wait_headers
#include <sys/wait.h>
"
else
AC_HAVE_HEADERS(wait.h)
AC_HAVE_HEADERS(waitstatus.h)
if test $ac_cv_header_wait_h = yes; then
cf_wait_headers="$cf_wait_headers
#include <wait.h>
"
fi
if test $ac_cv_header_waitstatus_h = yes; then
cf_wait_headers="$cf_wait_headers
#include <waitstatus.h>
"
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Wrapper for AC_ARG_WITH to ensure that user supplies a pathname, not just
dnl defaulting to yes/no.
dnl
dnl $1 = option name
dnl $2 = help-text
dnl $3 = environment variable to set
dnl $4 = default value, shown in the help-message, must be a constant
dnl $5 = default value, if it's an expression & cannot be in the help-message
dnl
AC_DEFUN([CF_WITH_PATH],
[AC_ARG_WITH($1,[$2 ](default: ifelse($4,,empty,$4)),,
ifelse($4,,[withval="${$3}"],[withval="${$3-ifelse($5,,$4,$5)}"]))dnl
CF_PATH_SYNTAX(withval)
eval $3="$withval"
AC_SUBST($3)dnl
])dnl
