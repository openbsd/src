dnl
dnl $KTH: bsd-func-lockstatus.m4,v 1.6 2000/03/24 03:39:42 assar Exp $
dnl

AC_DEFUN(AC_BSD_FUNC_LOCKSTATUS, [
AC_CHECK_KERNEL_FUNCS(lockstatus)
if test "$ac_cv_kernel_func_lockstatus" = "yes"; then
AC_CACHE_CHECK(if lockstatus takes two arguments,
ac_cv_func_lockstatus_two_args,
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/lock.h>
], [lockstatus(NULL, NULL)],
ac_cv_func_lockstatus_two_args=yes,
ac_cv_func_lockstatus_two_args=no))
if test "$ac_cv_func_lockstatus_two_args" = yes; then
	AC_DEFINE(HAVE_TWO_ARGUMENT_LOCKSTATUS, 1,
	[define if lockstatus takes two arguments])
fi

AC_CACHE_CHECK(if lockstatus takes one argument,
ac_cv_func_lockstatus_one_args,
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
], [lockstatus(NULL)],
ac_cv_func_lockstatus_one_args=yes,
ac_cv_func_lockstatus_one_args=no))
if test "$ac_cv_func_lockstatus_one_args" = yes; then
	AC_DEFINE(HAVE_ONE_ARGUMENT_LOCKSTATUS, 1,
	[define if lockstatus takes one argument])
fi

if test "$ac_cv_func_lockstatus_two_args" = "no" -a "$ac_cv_func_lockstatus_one_args" = "no"; then
	AC_MSG_ERROR([unable to figure out how many args lockstatus takes])
fi
fi
])
