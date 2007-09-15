#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([pcc], [0.9.8], BUG-REPORT-ADDRESS)
AC_CONFIG_HEADER([config.h])

AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_CANONICAL_TARGET


case "$target_os" in
    netbsd*)
	targos=netbsd
	case "$target_cpu" in
	    i?86) targmach=x86 ;;
	esac
	;;
    openbsd*)
	targos=openbsd
	case "$target_cpu" in
	    i?86) targmach=x86 ;;
	esac
	;;
    dragonfly*)
	targos=dragonfly
	case "$target_cpu" in
	    i?86) targmach=x86 ;;
	esac
	;;
    linux*)
	targos=linux
	case "$target_cpu" in
	    i?86) targmach=x86 ;;
	esac
	;;
    *)
        targos="$target_os"
	case "$target_cpu" in
	    m16c) targmach=m16c ;;
	    nova) targmach=nova ;;
	esac
	;;
esac
   
if test "X$targos" = X -o "X$targmach" = X ; then
	AC_MSG_ERROR(['$target' is not (yet) supported by pcc.])
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_LEX
AC_PROG_YACC

AC_CHECK_PROG(strip,strip,yes,no)

# Checks for libraries.

# Checks for header files.
# AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])
AC_CHECK_HEADERS([string.h alloca.h])

# Checks for library functions.
AC_FUNC_ALLOCA
##  AC_FUNC_STRTOD
# AC_FUNC_VPRINTF
# AC_CHECK_FUNCS([memset strchr strdup strrchr strtol])
AC_CHECK_FUNCS(mkstemp)

AC_SUBST(targos)
AC_SUBST(targmach)
AC_SUBST(prefix)
AC_SUBST(exec_prefix)
AC_SUBST(libexecdir)
AC_SUBST(includedir)
AC_SUBST(strip)

pcc_major=`echo $PACKAGE_VERSION | awk -F. '{print $1}'`
pcc_minor=`echo $PACKAGE_VERSION | awk -F. '{print $2}'`
pcc_minorminor=`echo $PACKAGE_VERSION | awk -F. '{print $3}'`
versstr="\"$PACKAGE_STRING for $target, $USER@`hostname` `date`\""

AC_DEFINE_UNQUOTED(TARGOS, $targos)
AC_DEFINE_UNQUOTED(PCC_MAJOR, $pcc_major)
AC_DEFINE_UNQUOTED(PCC_MINOR, $pcc_minor)
AC_DEFINE_UNQUOTED(PCC_MINORMINOR, $pcc_minorminor)
AC_DEFINE_UNQUOTED(VERSSTR, $versstr)
if test "$LEX" = flex ; then
	AC_DEFINE_UNQUOTED(ISFLEX, 1)
fi

AC_CONFIG_FILES([Makefile
		cc/Makefile
		cc/cc/Makefile
		cc/cpp/Makefile
		cc/ccom/Makefile
		f77/Makefile
		f77/f77/Makefile
		f77/fcom/Makefile
])
AC_OUTPUT
