dnl
dnl $KTH: linux-func-d_alloc_root-two_args.m4,v 1.2 1999/07/22 04:59:34 assar Exp $
dnl

AC_DEFUN(AC_LINUX_FUNC_D_ALLOC_ROOT_TWO_ARGS, [
AC_CACHE_CHECK(if d_alloc_root takes two arguments,
ac_cv_func_d_alloc_root_two_args,
AC_TRY_COMPILE_KERNEL([#include <asm/current.h>
#include <linux/fs.h>
#include <linux/dcache.h>],
[
d_alloc_root(NULL, NULL)
],
ac_cv_func_d_alloc_root_two_args=yes,
ac_cv_func_d_alloc_root_two_args=no))

if test "$ac_cv_func_d_alloc_root_two_args" = "yes"; then
  AC_DEFINE(HAVE_D_ALLOC_ROOT_TWO_ARGS, 1,
	[define if d_alloc_root takes two arguments])
fi
])
