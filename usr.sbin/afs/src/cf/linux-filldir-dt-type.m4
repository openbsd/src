dnl
dnl $KTH: linux-filldir-dt-type.m4,v 1.1.4.1 2001/05/07 01:29:59 ahltorp Exp $
dnl

AC_DEFUN(AC_LINUX_FILLDIR_DT_TYPE, [
AC_CACHE_CHECK([for whether filldir_t includes a dt_type],
ac_cv_type_filldir_dt_type,
AC_TRY_COMPILE_KERNEL([#include <asm/current.h>
#include <linux/fs.h>],
[
filldir_t bar;

bar(0, 0, 0, 0, 0, 0);
],
ac_cv_type_filldir_dt_type=yes,
ac_cv_type_filldir_dt_type=no))

if test "$ac_cv_type_filldir_dt_type" = "yes"; then
  AC_DEFINE(HAVE_FILLDIR_T_DT_TYPE, 1, [define if filldir_t takes a dt_type argument])
fi
])
