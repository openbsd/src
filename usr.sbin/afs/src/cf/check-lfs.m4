dnl
dnl $KTH: check-lfs.m4,v 1.2 2000/03/14 00:04:10 assar Exp $
dnl

AC_DEFUN(AC_HAVE_GETCONF, [
AC_CHECK_PROG(ac_cv_prog_getconf,[getconf],yes)
])

AC_DEFUN(AC_GETCONF_FLAGS,[
if test "$ac_cv_prog_getconf" = "yes";then
AC_MSG_CHECKING([if we have $1])
FOO="`getconf $1 2>/dev/null >/dev/null`"
if test $? = 0 ;then
$2="[$]$2 $FOO"
AC_MSG_RESULT([yes $FOO])
else
AC_MSG_RESULT(no)
fi
fi

])

AC_DEFUN(AC_CHECK_LFS, [
if test "${ac_cv_prog_getconf-set}" = set ;then
AC_HAVE_GETCONF
fi
AC_GETCONF_FLAGS(LFS_CFLAGS, CFLAGS)
AC_GETCONF_FLAGS(LFS_LDFLAGS, LDFLAGS)
])


