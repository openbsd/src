dnl
dnl $Id: func-getvfsbyname.m4,v 1.1 2000/09/11 14:40:47 art Exp $
dnl

AC_DEFUN(AC_FUNC_GETVFSBYNAME, [
AC_CHECK_FUNCS(getvfsbyname)
if test "$ac_cv_func_getvfsbyname" = "yes"; then
AC_CACHE_CHECK(for two argument getvfsbyname,
ac_cv_func_getvfsbyname_two_arguments,
AC_TRY_COMPILE(
[#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>],
[struct vfsconf vfc;
int foo = getvfsbyname("arla", &vfc);
],
ac_cv_func_getvfsbyname_two_arguments=yes,
ac_cv_func_getvfsbyname_two_arguments=no))
if test "$ac_cv_func_getvfsbyname_two_arguments" = "yes"; then
  AC_DEFINE(HAVE_GETVFSBYNAME_TWO_ARGS, 1,
	[define if getvfsbyname takes two arguments])
fi
fi
])
