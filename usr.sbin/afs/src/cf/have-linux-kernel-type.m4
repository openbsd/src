dnl
dnl $KTH: have-linux-kernel-type.m4,v 1.2 1999/12/05 10:33:09 assar Exp $
dnl
dnl Check for types in the Linux kernel
dnl

dnl AC_HAVE_LINUX_KERNEL_TYPE(type)
AC_DEFUN(AC_HAVE_LINUX_KERNEL_TYPE, [
cv=`echo "$1" | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
AC_MSG_CHECKING(for $i in the linux kernel)
AC_CACHE_VAL(ac_cv_linux_kernel_type_$cv,
AC_TRY_COMPILE_KERNEL([
#define __KERNEL__
#include <linux/types.h>
],
[$1 x;],
eval "ac_cv_linux_kernel_type_$cv=yes",
eval "ac_cv_linux_kernel_type_$cv=no"))dnl
AC_MSG_RESULT(`eval echo \\$ac_cv_linux_kernel_type_$cv`)
if test `eval echo \\$ac_cv_linux_kernel_type_$cv` = yes; then
  ac_tr_hdr=HAVE_LINUX_KERNEL_`echo $1 | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
  AC_DEFINE_UNQUOTED($ac_tr_hdr, 1)
fi
])
