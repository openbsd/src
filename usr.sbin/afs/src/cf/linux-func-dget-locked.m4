dnl
dnl $KTH: linux-func-dget-locked.m4,v 1.1 2000/09/14 07:19:31 assar Exp $
dnl

AC_DEFUN(AC_LINUX_FUNC_DGET_LOCKED, [
AC_CACHE_CHECK([for dget_locked],
ac_cv_func_dget_locked, [
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $test_KERNEL_CFLAGS $KERNEL_CPPFLAGS"
AC_EGREP_CPP([dget_locked],
[#include <linux/dcache.h>],
ac_cv_func_dget_locked=yes,
ac_cv_func_dget_locked=no)]
CPPFLAGS="$save_CPPFLAGS"
)
if test "$ac_cv_func_dget_locked" = "yes"; then
  AC_DEFINE(HAVE_DGET_LOCKED, 1,
	[define if you have a function dget_locked])
fi
])
