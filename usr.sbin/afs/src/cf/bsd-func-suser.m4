dnl
dnl $KTH: bsd-func-suser.m4,v 1.4 2000/11/01 02:09:16 assar Exp $
dnl

AC_DEFUN(AC_BSD_FUNC_SUSER, [
AC_CACHE_CHECK(if suser takes two arguments,
ac_cv_func_suser_two_args,
AC_TRY_COMPILE_KERNEL([
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/systm.h>
], [suser(NULL, NULL)],
ac_cv_func_suser_two_args=yes,
ac_cv_func_suser_two_args=no))
if test "$ac_cv_func_suser_two_args" = yes; then
	AC_DEFINE(HAVE_TWO_ARGUMENT_SUSER, 1,
	[define if suser takes two arguments])
fi
])
