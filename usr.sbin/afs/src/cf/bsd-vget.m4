dnl
dnl $KTH: bsd-vget.m4,v 1.4 2000/06/12 06:13:52 assar Exp $
dnl

AC_DEFUN(AC_BSD_FUNC_VGET, [
AC_CACHE_CHECK(if vget takes one argument, ac_cv_func_vget_one_args,
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#if defined(__osf__) && defined(__GNUC__)
#define asm __foo_asm
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vnode.h>
],[vget(0)],
ac_cv_func_vget_one_args=yes,
ac_cv_func_vget_one_args=no))
if test "$ac_cv_func_vget_one_args" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_ONE_ARGUMENT_VGET, 1,
	[define if vget takes one argument])
fi

AC_CACHE_CHECK(if vget takes two arguments, ac_cv_func_vget_two_args,
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vnode.h>
],[vget(0, 0)],
ac_cv_func_vget_two_args=yes,
ac_cv_func_vget_two_args=no))
if test "$ac_cv_func_vget_two_args" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_TWO_ARGUMENT_VGET, 1,
	[define if vget takes two arguments])
fi

AC_CACHE_CHECK(if vget takes three arguments, ac_cv_func_vget_three_args,
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vnode.h>
],[vget(0, 0, 0)],
ac_cv_func_vget_three_args=yes,
ac_cv_func_vget_three_args=no))
if test "$ac_cv_func_vget_three_args" = yes; then
	AC_DEFINE(HAVE_THREE_ARGUMENT_VGET, 1,
	[define if vget takes three arguments])
fi

if test "$ac_cv_func_vget_one_args" = "no" -a "$ac_cv_func_vget_two_args" = "no" -a "$ac_cv_func_vget_three_args" = "no"; then
  AC_MSG_ERROR([test for number of arguments of vget failed])
fi
])
